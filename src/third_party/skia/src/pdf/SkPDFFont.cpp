/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkPDFFont.h"

#include "SkData.h"
#include "SkFont.h"
#include "SkGlyphCache.h"
#include "SkImagePriv.h"
#include "SkMacros.h"
#include "SkMakeUnique.h"
#include "SkPDFBitmap.h"
#include "SkPDFCanon.h"
#include "SkPDFConvertType1FontStream.h"
#include "SkPDFDevice.h"
#include "SkPDFDocumentPriv.h"
#include "SkPDFMakeCIDGlyphWidthsArray.h"
#include "SkPDFMakeToUnicodeCmap.h"
#include "SkPDFResourceDict.h"
#include "SkPDFSubsetFont.h"
#include "SkPDFUtils.h"
#include "SkPaint.h"
#include "SkRefCnt.h"
#include "SkScalar.h"
#include "SkStream.h"
#include "SkTo.h"
#include "SkTypes.h"
#include "SkUTF.h"

SkExclusiveStrikePtr SkPDFFont::MakeVectorCache(SkTypeface* face, int* size) {
    SkFont font;
    font.setHinting(kNo_SkFontHinting);
    font.setEdging(SkFont::Edging::kAlias);
    font.setTypeface(sk_ref_sp(face));
    int unitsPerEm = face->getUnitsPerEm();
    if (unitsPerEm <= 0) {
        unitsPerEm = 1024;
    }
    if (size) {
        *size = unitsPerEm;
    }
    font.setSize((SkScalar)unitsPerEm);
    const SkSurfaceProps props(0, kUnknown_SkPixelGeometry);
    return SkStrikeCache::FindOrCreateStrikeExclusive(
        font, SkPaint(), props, SkScalerContextFlags::kFakeGammaAndBoostContrast, SkMatrix::I());
}

namespace {
// PDF's notion of symbolic vs non-symbolic is related to the character set, not
// symbols vs. characters.  Rarely is a font the right character set to call it
// non-symbolic, so always call it symbolic.  (PDF 1.4 spec, section 5.7.1)
static const int32_t kPdfSymbolic = 4;


// scale from em-units to base-1000, returning as a SkScalar
SkScalar from_font_units(SkScalar scaled, uint16_t emSize) {
    if (emSize == 1000) {
        return scaled;
    } else {
        return scaled * 1000 / emSize;
    }
}

SkScalar scaleFromFontUnits(int16_t val, uint16_t emSize) {
    return from_font_units(SkIntToScalar(val), emSize);
}


void setGlyphWidthAndBoundingBox(SkScalar width, SkIRect box,
                                 SkDynamicMemoryWStream* content) {
    // Specify width and bounding box for the glyph.
    SkPDFUtils::AppendScalar(width, content);
    content->writeText(" 0 ");
    content->writeDecAsText(box.fLeft);
    content->writeText(" ");
    content->writeDecAsText(box.fTop);
    content->writeText(" ");
    content->writeDecAsText(box.fRight);
    content->writeText(" ");
    content->writeDecAsText(box.fBottom);
    content->writeText(" d1\n");
}
}  // namespace

///////////////////////////////////////////////////////////////////////////////
// class SkPDFFont
///////////////////////////////////////////////////////////////////////////////

/* Resources are canonicalized and uniqueified by pointer so there has to be
 * some additional state indicating which subset of the font is used.  It
 * must be maintained at the document granularity.
 */

SkPDFFont::~SkPDFFont() = default;

SkPDFFont::SkPDFFont(SkPDFFont&&) = default;

SkPDFFont& SkPDFFont::operator=(SkPDFFont&&) = default;

static bool can_embed(const SkAdvancedTypefaceMetrics& metrics) {
    return !SkToBool(metrics.fFlags & SkAdvancedTypefaceMetrics::kNotEmbeddable_FontFlag);
}

const SkAdvancedTypefaceMetrics* SkPDFFont::GetMetrics(const SkTypeface* typeface,
                                                       SkPDFCanon* canon) {
    SkASSERT(typeface);
    SkFontID id = typeface->uniqueID();
    if (std::unique_ptr<SkAdvancedTypefaceMetrics>* ptr = canon->fTypefaceMetrics.find(id)) {
        return ptr->get();  // canon retains ownership.
    }
    int count = typeface->countGlyphs();
    if (count <= 0 || count > 1 + SkTo<int>(UINT16_MAX)) {
        // Cache nullptr to skip this check.  Use SkSafeUnref().
        canon->fTypefaceMetrics.set(id, nullptr);
        return nullptr;
    }
    std::unique_ptr<SkAdvancedTypefaceMetrics> metrics = typeface->getAdvancedMetrics();
    if (!metrics) {
        metrics = skstd::make_unique<SkAdvancedTypefaceMetrics>();
    }

    if (0 == metrics->fStemV || 0 == metrics->fCapHeight) {
        SkPaint tmpPaint;
        tmpPaint.setHinting(kNo_SkFontHinting);
        tmpPaint.setTypeface(sk_ref_sp(typeface));
        tmpPaint.setTextSize(1000);  // glyph coordinate system
        if (0 == metrics->fStemV) {
            // Figure out a good guess for StemV - Min width of i, I, !, 1.
            // This probably isn't very good with an italic font.
            int16_t stemV = SHRT_MAX;
            for (char c : {'i', 'I', '!', '1'}) {
                SkRect bounds;
                tmpPaint.measureText(&c, 1, &bounds);
                stemV = SkTMin(stemV, SkToS16(SkScalarRoundToInt(bounds.width())));
            }
            metrics->fStemV = stemV;
        }
        if (0 == metrics->fCapHeight) {
            // Figure out a good guess for CapHeight: average the height of M and X.
            SkScalar capHeight = 0;
            for (char c : {'M', 'X'}) {
                SkRect bounds;
                tmpPaint.measureText(&c, 1, &bounds);
                capHeight += bounds.height();
            }
            metrics->fCapHeight = SkToS16(SkScalarRoundToInt(capHeight / 2));
        }
    }
    return canon->fTypefaceMetrics.set(id, std::move(metrics))->get();
}

const std::vector<SkUnichar>& SkPDFFont::GetUnicodeMap(const SkTypeface* typeface,
                                                       SkPDFCanon* canon) {
    SkASSERT(typeface);
    SkASSERT(canon);
    SkFontID id = typeface->uniqueID();
    if (std::vector<SkUnichar>* ptr = canon->fToUnicodeMap.find(id)) {
        return *ptr;
    }
    std::vector<SkUnichar> buffer(typeface->countGlyphs());
    typeface->getGlyphToUnicodeMap(buffer.data());
    return *canon->fToUnicodeMap.set(id, std::move(buffer));
}

SkAdvancedTypefaceMetrics::FontType SkPDFFont::FontType(const SkAdvancedTypefaceMetrics& metrics) {
    if (SkToBool(metrics.fFlags & SkAdvancedTypefaceMetrics::kMultiMaster_FontFlag) ||
        SkToBool(metrics.fFlags & SkAdvancedTypefaceMetrics::kNotEmbeddable_FontFlag)) {
        // force Type3 fallback.
        return SkAdvancedTypefaceMetrics::kOther_Font;
    }
    return metrics.fType;
}

static SkGlyphID first_nonzero_glyph_for_single_byte_encoding(SkGlyphID gid) {
    return gid != 0 ? gid - (gid - 1) % 255 : 1;
}

static bool has_outline_glyph(SkGlyphID gid, SkGlyphCache* cache) {
    const SkGlyph& glyph = cache->getGlyphIDMetrics(gid);
    return glyph.isEmpty() || cache->findPath(glyph);
}

SkPDFFont* SkPDFFont::GetFontResource(SkPDFDocument* doc,
                                      SkGlyphCache* cache,
                                      SkTypeface* face,
                                      SkGlyphID glyphID) {
    SkASSERT(doc);
    SkPDFCanon* canon = doc->canon();
    SkASSERT(canon);
    SkASSERT(face);  // All SkPDFDevice::internalDrawText ensures this.
    const SkAdvancedTypefaceMetrics* fontMetrics = SkPDFFont::GetMetrics(face, canon);
    SkASSERT(fontMetrics);  // SkPDFDevice::internalDrawText ensures the typeface is good.
                            // GetMetrics only returns null to signify a bad typeface.
    const SkAdvancedTypefaceMetrics& metrics = *fontMetrics;
    SkAdvancedTypefaceMetrics::FontType type = SkPDFFont::FontType(metrics);
    if (!has_outline_glyph(glyphID, cache)) {
        type = SkAdvancedTypefaceMetrics::kOther_Font;
    }
    bool multibyte = SkPDFFont::IsMultiByte(type);
    SkGlyphID subsetCode = multibyte ? 0 : first_nonzero_glyph_for_single_byte_encoding(glyphID);
    uint64_t fontID = (static_cast<uint64_t>(SkTypeface::UniqueID(face)) << 16) | subsetCode;

    if (SkPDFFont* found = canon->fFontMap.find(fontID)) {
        SkASSERT(multibyte == found->multiByteGlyphs());
        return found;
    }

    sk_sp<SkTypeface> typeface(sk_ref_sp(face));
    SkASSERT(typeface);

    SkGlyphID lastGlyph = SkToU16(typeface->countGlyphs() - 1);

    // should be caught by SkPDFDevice::internalDrawText
    SkASSERT(glyphID <= lastGlyph);

    SkGlyphID firstNonZeroGlyph;
    if (multibyte) {
        firstNonZeroGlyph = 1;
    } else {
        firstNonZeroGlyph = subsetCode;
        lastGlyph = SkToU16(SkTMin<int>((int)lastGlyph, 254 + (int)subsetCode));
    }
    auto ref = doc->reserve();
    return canon->fFontMap.set(
            fontID, SkPDFFont(std::move(typeface), firstNonZeroGlyph, lastGlyph, type, ref));
}

SkPDFFont::SkPDFFont(sk_sp<SkTypeface> typeface,
                     SkGlyphID firstGlyphID,
                     SkGlyphID lastGlyphID,
                     SkAdvancedTypefaceMetrics::FontType fontType,
                     SkPDFIndirectReference indirectReference)
    : fTypeface(std::move(typeface))
    , fGlyphUsage(firstGlyphID, lastGlyphID)
    , fIndirectReference(indirectReference)
    , fFontType(fontType) {}

static void add_common_font_descriptor_entries(SkPDFDict* descriptor,
                                               const SkAdvancedTypefaceMetrics& metrics,
                                               uint16_t emSize,
                                               int16_t defaultWidth) {
    descriptor->insertName("FontName", metrics.fPostScriptName);
    descriptor->insertInt("Flags", (size_t)(metrics.fStyle | kPdfSymbolic));
    descriptor->insertScalar("Ascent",
            scaleFromFontUnits(metrics.fAscent, emSize));
    descriptor->insertScalar("Descent",
            scaleFromFontUnits(metrics.fDescent, emSize));
    descriptor->insertScalar("StemV",
            scaleFromFontUnits(metrics.fStemV, emSize));
    descriptor->insertScalar("CapHeight",
            scaleFromFontUnits(metrics.fCapHeight, emSize));
    descriptor->insertInt("ItalicAngle", metrics.fItalicAngle);
    descriptor->insertObject("FontBBox",
                             SkPDFMakeArray(scaleFromFontUnits(metrics.fBBox.left(), emSize),
                                            scaleFromFontUnits(metrics.fBBox.bottom(), emSize),
                                            scaleFromFontUnits(metrics.fBBox.right(), emSize),
                                            scaleFromFontUnits(metrics.fBBox.top(), emSize)));
    if (defaultWidth > 0) {
        descriptor->insertScalar("MissingWidth",
                scaleFromFontUnits(defaultWidth, emSize));
    }
}

///////////////////////////////////////////////////////////////////////////////
//  Type0Font
///////////////////////////////////////////////////////////////////////////////

#ifdef SK_PDF_SUBSET_SUPPORTED

// if possible, make no copy.
static sk_sp<SkData> stream_to_data(std::unique_ptr<SkStreamAsset> stream) {
    SkASSERT(stream);
    (void)stream->rewind();
    SkASSERT(stream->hasLength());
    size_t size = stream->getLength();
    if (const void* base = stream->getMemoryBase()) {
        SkData::ReleaseProc proc =
            [](const void*, void* ctx) { delete (SkStreamAsset*)ctx; };
        return SkData::MakeWithProc(base, size, proc, stream.release());
    }
    return SkData::MakeFromStream(stream.get(), size);
}
#endif  // SK_PDF_SUBSET_SUPPORTED

static void emit_subset_type0(const SkPDFFont& font, SkPDFDocument* doc) {
    SkPDFCanon* canon = doc->canon();
    const SkAdvancedTypefaceMetrics* metricsPtr =
        SkPDFFont::GetMetrics(font.typeface(), canon);
    SkASSERT(metricsPtr);
    if (!metricsPtr) { return; }
    const SkAdvancedTypefaceMetrics& metrics = *metricsPtr;
    SkASSERT(can_embed(metrics));
    SkAdvancedTypefaceMetrics::FontType type = font.getType();
    SkTypeface* face = font.typeface();
    SkASSERT(face);

    auto descriptor = sk_make_sp<SkPDFDict>("FontDescriptor");
    uint16_t emSize = SkToU16(font.typeface()->getUnitsPerEm());
    add_common_font_descriptor_entries(descriptor.get(), metrics, emSize , 0);

    int ttcIndex;
    std::unique_ptr<SkStreamAsset> fontAsset(face->openStream(&ttcIndex));
    size_t fontSize = fontAsset ? fontAsset->getLength() : 0;
    if (0 == fontSize) {
        SkDebugf("Error: (SkTypeface)(%p)::openStream() returned "
                 "empty stream (%p) when identified as kType1CID_Font "
                 "or kTrueType_Font.\n", face, fontAsset.get());
    } else {
        switch (type) {
            case SkAdvancedTypefaceMetrics::kTrueType_Font: {
                #ifdef SK_PDF_SUBSET_SUPPORTED
                if (!SkToBool(metrics.fFlags &
                              SkAdvancedTypefaceMetrics::kNotSubsettable_FontFlag)) {
                    SkASSERT(font.firstGlyphID() == 1);
                    sk_sp<SkData> subsetFontData = SkPDFSubsetFont(
                            stream_to_data(std::move(fontAsset)), font.glyphUsage(),
                            metrics.fFontName.c_str(), ttcIndex);
                    if (subsetFontData) {
                        size_t len = subsetFontData->size();
                        sk_sp<SkPDFStream> subsetStream = sk_make_sp<SkPDFStream>(
                                std::move(subsetFontData));
                        subsetStream->dict()->insertInt("Length1", SkToInt(len));
                        descriptor->insertRef("FontFile2", doc->serialize(subsetStream));
                        break;
                    }
                    // If subsetting fails, fall back to original font data.
                    fontAsset.reset(face->openStream(&ttcIndex));
                    SkASSERT(fontAsset);
                    SkASSERT(fontAsset->getLength() == fontSize);
                    if (!fontAsset || fontAsset->getLength() == 0) { break; }
                }
                #endif  // SK_PDF_SUBSET_SUPPORTED
                auto fontStream = sk_make_sp<SkPDFSharedStream>(std::move(fontAsset));
                fontStream->dict()->insertInt("Length1", fontSize);
                descriptor->insertRef("FontFile2", doc->serialize(fontStream));
                break;
            }
            case SkAdvancedTypefaceMetrics::kType1CID_Font: {
                auto fontStream = sk_make_sp<SkPDFSharedStream>(std::move(fontAsset));
                fontStream->dict()->insertName("Subtype", "CIDFontType0C");
                descriptor->insertRef("FontFile3", doc->serialize(fontStream));
                break;
            }
            default:
                SkASSERT(false);
        }
    }

    auto newCIDFont = sk_make_sp<SkPDFDict>("Font");
    newCIDFont->insertRef("FontDescriptor", doc->serialize(descriptor));
    newCIDFont->insertName("BaseFont", metrics.fPostScriptName);

    switch (type) {
        case SkAdvancedTypefaceMetrics::kType1CID_Font:
            newCIDFont->insertName("Subtype", "CIDFontType0");
            break;
        case SkAdvancedTypefaceMetrics::kTrueType_Font:
            newCIDFont->insertName("Subtype", "CIDFontType2");
            newCIDFont->insertName("CIDToGIDMap", "Identity");
            break;
        default:
            SkASSERT(false);
    }
    auto sysInfo = sk_make_sp<SkPDFDict>();
    sysInfo->insertString("Registry", "Adobe");
    sysInfo->insertString("Ordering", "Identity");
    sysInfo->insertInt("Supplement", 0);
    newCIDFont->insertObject("CIDSystemInfo", std::move(sysInfo));

    int16_t defaultWidth = 0;
    {
        int emSize;
        auto glyphCache = SkPDFFont::MakeVectorCache(face, &emSize);
        sk_sp<SkPDFArray> widths = SkPDFMakeCIDGlyphWidthsArray(
                glyphCache.get(), &font.glyphUsage(), SkToS16(emSize), &defaultWidth);
        if (widths && widths->size() > 0) {
            newCIDFont->insertObject("W", std::move(widths));
        }
        newCIDFont->insertScalar(
                "DW", scaleFromFontUnits(defaultWidth, SkToS16(emSize)));
    }

    ////////////////////////////////////////////////////////////////////////////

    SkPDFDict fontDict("Font");
    fontDict.insertName("Subtype", "Type0");
    fontDict.insertName("BaseFont", metrics.fPostScriptName);
    fontDict.insertName("Encoding", "Identity-H");
    auto descendantFonts = sk_make_sp<SkPDFArray>();
    descendantFonts->appendRef(doc->serialize(newCIDFont));
    fontDict.insertObject("DescendantFonts", std::move(descendantFonts));

    const std::vector<SkUnichar>& glyphToUnicode =
        SkPDFFont::GetUnicodeMap(font.typeface(), canon);
    SkASSERT(SkToSizeT(font.typeface()->countGlyphs()) == glyphToUnicode.size());
    fontDict.insertRef("ToUnicode",
                    doc->serialize(
                       SkPDFMakeToUnicodeCmap(glyphToUnicode.data(),
                                              &font.glyphUsage(),
                                              font.multiByteGlyphs(),
                                              font.firstGlyphID(),
                                              font.lastGlyphID())));

    SkWStream* stream = doc->beginObject(font.indirectReference());
    fontDict.emitObject(stream);
    doc->endObject();
}

///////////////////////////////////////////////////////////////////////////////
// Type1Font
///////////////////////////////////////////////////////////////////////////////

static SkPDFIndirectReference make_type1_font_descriptor(SkPDFDocument* doc,
                                                         const SkTypeface* typeface,
                                                         const SkAdvancedTypefaceMetrics* info) {
    SkPDFDict descriptor("FontDescriptor");
    uint16_t emSize = SkToU16(typeface->getUnitsPerEm());
    if (info) {
        add_common_font_descriptor_entries(&descriptor, *info, emSize, 0);
        if (can_embed(*info)) {
            int ttcIndex;
            size_t header SK_INIT_TO_AVOID_WARNING;
            size_t data SK_INIT_TO_AVOID_WARNING;
            size_t trailer SK_INIT_TO_AVOID_WARNING;
            std::unique_ptr<SkStreamAsset> rawFontData(typeface->openStream(&ttcIndex));
            sk_sp<SkData> fontData = SkPDFConvertType1FontStream(std::move(rawFontData),
                                                                 &header, &data, &trailer);
            if (fontData) {
                SkPDFStream fontStream(std::move(fontData));
                fontStream.dict()->insertInt("Length1", header);
                fontStream.dict()->insertInt("Length2", data);
                fontStream.dict()->insertInt("Length3", trailer);
                descriptor.insertRef("FontFile", doc->emit(fontStream));
            }
        }
    }
    return doc->emit(descriptor);
}


static const std::vector<SkString>& type_1_glyphnames(SkPDFCanon* canon,
                                                      const SkTypeface* typeface) {
    SkFontID fontID = typeface->uniqueID();
    const std::vector<SkString>* glyphNames = canon->fType1GlyphNames.find(fontID);
    if (!glyphNames) {
        std::vector<SkString> names(typeface->countGlyphs());
        SkPDFFont::GetType1GlyphNames(*typeface, names.data());
        glyphNames = canon->fType1GlyphNames.set(fontID, std::move(names));
    }
    SkASSERT(glyphNames);
    return *glyphNames;
}

static SkPDFIndirectReference type1_font_descriptor(SkPDFDocument* doc,
                                                    const SkTypeface* typeface) {
    SkFontID fontID = typeface->uniqueID();
    SkPDFCanon* canon = doc->canon();
    if (SkPDFIndirectReference* ptr = canon->fFontDescriptors.find(fontID)) {
        return *ptr;
    }
    const SkAdvancedTypefaceMetrics* info = SkPDFFont::GetMetrics(typeface, canon);
    auto fontDescriptor = make_type1_font_descriptor(doc, typeface, info);
    canon->fFontDescriptors.set(fontID, fontDescriptor);
    return fontDescriptor;
}

static void emit_subset_type1(const SkPDFFont& pdfFont, SkPDFDocument* doc) {
    SkTypeface* typeface = pdfFont.typeface();
    const std::vector<SkString> glyphNames = type_1_glyphnames(doc->canon(), typeface);
    SkGlyphID firstGlyphID = pdfFont.firstGlyphID();
    SkGlyphID lastGlyphID = pdfFont.lastGlyphID();

    SkPDFDict font("Font");
    font.insertRef("FontDescriptor", type1_font_descriptor(doc, typeface));
    font.insertName("Subtype", "Type1");
    if (const SkAdvancedTypefaceMetrics* info = SkPDFFont::GetMetrics(typeface, doc->canon())) {
        font.insertName("BaseFont", info->fPostScriptName);
    }

    // glyphCount not including glyph 0
    unsigned glyphCount = 1 + lastGlyphID - firstGlyphID;
    SkASSERT(glyphCount > 0 && glyphCount <= 255);
    font.insertInt("FirstChar", (size_t)0);
    font.insertInt("LastChar", (size_t)glyphCount);
    {
        int emSize;
        auto glyphCache = SkPDFFont::MakeVectorCache(typeface, &emSize);
        auto widths = sk_make_sp<SkPDFArray>();
        SkScalar advance = glyphCache->getGlyphIDAdvance(0).fAdvanceX;
        widths->appendScalar(from_font_units(advance, SkToU16(emSize)));
        for (unsigned gID = firstGlyphID; gID <= lastGlyphID; gID++) {
            advance = glyphCache->getGlyphIDAdvance(gID).fAdvanceX;
            widths->appendScalar(from_font_units(advance, SkToU16(emSize)));
        }
        font.insertObject("Widths", std::move(widths));
    }
    auto encDiffs = sk_make_sp<SkPDFArray>();
    encDiffs->reserve(lastGlyphID - firstGlyphID + 3);
    encDiffs->appendInt(0);

    SkASSERT(glyphNames.size() > lastGlyphID);
    const SkString unknown("UNKNOWN");
    encDiffs->appendName(glyphNames[0].isEmpty() ? unknown : glyphNames[0]);
    for (int gID = firstGlyphID; gID <= lastGlyphID; gID++) {
        encDiffs->appendName(glyphNames[gID].isEmpty() ? unknown : glyphNames[gID]);
    }

    auto encoding = sk_make_sp<SkPDFDict>("Encoding");
    encoding->insertObject("Differences", std::move(encDiffs));
    font.insertObject("Encoding", std::move(encoding));

    SkWStream* stream = doc->beginObject(pdfFont.indirectReference());
    font.emitObject(stream);
    doc->endObject();
}

void SkPDFFont::GetType1GlyphNames(const SkTypeface& face, SkString* dst) {
    face.getPostScriptGlyphNames(dst);
}

///////////////////////////////////////////////////////////////////////////////
// PDFType3Font
///////////////////////////////////////////////////////////////////////////////

namespace {
// returns [0, first, first+1, ... last-1,  last]
struct SingleByteGlyphIdIterator {
    SingleByteGlyphIdIterator(SkGlyphID first, SkGlyphID last)
        : fFirst(first), fLast(last) {
        SkASSERT(fFirst > 0);
        SkASSERT(fLast >= first);
    }
    struct Iter {
        void operator++() {
            fCurrent = (0 == fCurrent) ? fFirst : fCurrent + 1;
        }
        // This is an input_iterator
        SkGlyphID operator*() const { return (SkGlyphID)fCurrent; }
        bool operator!=(const Iter& rhs) const {
            return fCurrent != rhs.fCurrent;
        }
        Iter(SkGlyphID f, int c) : fFirst(f), fCurrent(c) {}
    private:
        const SkGlyphID fFirst;
        int fCurrent; // must be int to make fLast+1 to fit
    };
    Iter begin() const { return Iter(fFirst, 0); }
    Iter end() const { return Iter(fFirst, (int)fLast + 1); }
private:
    const SkGlyphID fFirst;
    const SkGlyphID fLast;
};
}

struct ImageAndOffset {
    sk_sp<SkImage> fImage;
    SkIPoint fOffset;
};
static ImageAndOffset to_image(SkGlyphID gid, SkGlyphCache* cache) {
    (void)cache->findImage(cache->getGlyphIDMetrics(gid));
    SkMask mask;
    cache->getGlyphIDMetrics(gid).toMask(&mask);
    if (!mask.fImage) {
        return {nullptr, {0, 0}};
    }
    SkIRect bounds = mask.fBounds;
    SkBitmap bm;
    switch (mask.fFormat) {
        case SkMask::kBW_Format:
            bm.allocPixels(SkImageInfo::MakeA8(bounds.width(), bounds.height()));
            for (int y = 0; y < bm.height(); ++y) {
                for (int x8 = 0; x8 < bm.width(); x8 += 8) {
                    uint8_t v = *mask.getAddr1(x8 + bounds.x(), y + bounds.y());
                    int e = SkTMin(x8 + 8, bm.width());
                    for (int x = x8; x < e; ++x) {
                        *bm.getAddr8(x, y) = (v >> (x & 0x7)) & 0x1 ? 0xFF : 0x00;
                    }
                }
            }
            bm.setImmutable();
            return {SkImage::MakeFromBitmap(bm), {bounds.x(), bounds.y()}};
        case SkMask::kA8_Format:
            bm.installPixels(SkImageInfo::MakeA8(bounds.width(), bounds.height()),
                             mask.fImage, mask.fRowBytes);
            return {SkMakeImageFromRasterBitmap(bm, kAlways_SkCopyPixelsMode),
                    {bounds.x(), bounds.y()}};
        case SkMask::kARGB32_Format:
            bm.installPixels(SkImageInfo::MakeN32Premul(bounds.width(), bounds.height()),
                             mask.fImage, mask.fRowBytes);
            return {SkMakeImageFromRasterBitmap(bm, kAlways_SkCopyPixelsMode),
                    {bounds.x(), bounds.y()}};
        case SkMask::k3D_Format:
        case SkMask::kLCD16_Format:
        default:
            SkASSERT(false);
            return {nullptr, {0, 0}};
    }
}

static SkPDFIndirectReference type3_descriptor(SkPDFDocument* doc,
                                               const SkTypeface* typeface,
                                               SkGlyphCache* cache) {
    SkPDFCanon* canon = doc->canon();
    if (SkPDFIndirectReference* ptr = canon->fType3FontDescriptors.find(typeface->uniqueID())) {
        return *ptr;
    }

    SkPDFDict descriptor("FontDescriptor");
    int32_t fontDescriptorFlags = kPdfSymbolic;
    if (const SkAdvancedTypefaceMetrics* metrics = SkPDFFont::GetMetrics(typeface, canon)) {
        // Type3 FontDescriptor does not require all the same fields.
        descriptor.insertName("FontName", metrics->fPostScriptName);
        descriptor.insertInt("ItalicAngle", metrics->fItalicAngle);
        fontDescriptorFlags |= (int32_t)metrics->fStyle;
        // Adobe requests CapHeight, XHeight, and StemV be added
        // to "greatly help our workflow downstream".
        if (metrics->fCapHeight != 0) { descriptor.insertInt("CapHeight", metrics->fCapHeight); }
        if (metrics->fStemV     != 0) { descriptor.insertInt("StemV",     metrics->fStemV);     }
        SkScalar xHeight = cache->getFontMetrics().fXHeight;
        if (xHeight != 0) {
            descriptor.insertScalar("XHeight", xHeight);
        }
    }
    descriptor.insertInt("Flags", fontDescriptorFlags);
    SkPDFIndirectReference ref = doc->emit(descriptor);
    canon->fType3FontDescriptors.set(typeface->uniqueID(), ref);
    return ref;
}


static void emit_subset_type3(const SkPDFFont& pdfFont, SkPDFDocument* doc) {
    SkTypeface* typeface = pdfFont.typeface();
    SkGlyphID firstGlyphID = pdfFont.firstGlyphID();
    SkGlyphID lastGlyphID = pdfFont.lastGlyphID();
    const SkPDFGlyphUse& subset = pdfFont.glyphUsage();
    SkPDFCanon* canon = doc->canon();
    SkASSERT(lastGlyphID >= firstGlyphID);
    // Remove unused glyphs at the end of the range.
    // Keep the lastGlyphID >= firstGlyphID invariant true.
    while (lastGlyphID > firstGlyphID && !subset.has(lastGlyphID)) {
        --lastGlyphID;
    }
    int unitsPerEm;
    auto cache = SkPDFFont::MakeVectorCache(typeface, &unitsPerEm);
    SkASSERT(cache);
    SkScalar emSize = (SkScalar)unitsPerEm;

    SkPDFDict font("Font");
    font.insertName("Subtype", "Type3");
    // Flip about the x-axis and scale by 1/emSize.
    SkMatrix fontMatrix;
    fontMatrix.setScale(SkScalarInvert(emSize), -SkScalarInvert(emSize));
    font.insertObject("FontMatrix", SkPDFUtils::MatrixToArray(fontMatrix));

    auto charProcs = sk_make_sp<SkPDFDict>();
    auto encoding = sk_make_sp<SkPDFDict>("Encoding");

    auto encDiffs = sk_make_sp<SkPDFArray>();
    // length(firstGlyphID .. lastGlyphID) ==  lastGlyphID - firstGlyphID + 1
    // plus 1 for glyph 0;
    SkASSERT(firstGlyphID > 0);
    SkASSERT(lastGlyphID >= firstGlyphID);
    int glyphCount = lastGlyphID - firstGlyphID + 2;
    // one other entry for the index of first glyph.
    encDiffs->reserve(glyphCount + 1);
    encDiffs->appendInt(0);  // index of first glyph

    auto widthArray = sk_make_sp<SkPDFArray>();
    widthArray->reserve(glyphCount);

    SkIRect bbox = SkIRect::MakeEmpty();

    std::vector<std::pair<SkGlyphID, SkPDFIndirectReference>> imageGlyphs;
    for (SkGlyphID gID : SingleByteGlyphIdIterator(firstGlyphID, lastGlyphID)) {
        bool skipGlyph = gID != 0 && !subset.has(gID);
        SkString characterName;
        SkScalar advance = 0.0f;
        SkIRect glyphBBox;
        if (skipGlyph) {
            characterName.set("g0");
        } else {
            characterName.printf("g%X", gID);
            const SkGlyph& glyph = cache->getGlyphIDMetrics(gID);
            advance = SkFloatToScalar(glyph.fAdvanceX);
            glyphBBox = SkIRect::MakeXYWH(glyph.fLeft, glyph.fTop,
                                          glyph.fWidth, glyph.fHeight);
            bbox.join(glyphBBox);
            const SkPath* path = cache->findPath(glyph);
            SkDynamicMemoryWStream content;
            if (path && !path->isEmpty()) {
                setGlyphWidthAndBoundingBox(SkFloatToScalar(glyph.fAdvanceX), glyphBBox, &content);
                SkPDFUtils::EmitPath(*path, SkPaint::kFill_Style, &content);
                SkPDFUtils::PaintPath(SkPaint::kFill_Style, path->getFillType(), &content);
            } else {
                auto pimg = to_image(gID, cache.get());
                if (!pimg.fImage) {
                    setGlyphWidthAndBoundingBox(SkFloatToScalar(glyph.fAdvanceX), glyphBBox,
                                                &content);
                } else {
                    imageGlyphs.emplace_back(gID, SkPDFSerializeImage(pimg.fImage.get(), doc));
                    SkPDFUtils::AppendScalar(SkFloatToScalar(glyph.fAdvanceX), &content);
                    content.writeText(" 0 d0\n");
                    content.writeDecAsText(pimg.fImage->width());
                    content.writeText(" 0 0 ");
                    content.writeDecAsText(-pimg.fImage->height());
                    content.writeText(" ");
                    content.writeDecAsText(pimg.fOffset.x());
                    content.writeText(" ");
                    content.writeDecAsText(pimg.fImage->height() + pimg.fOffset.y());
                    content.writeText(" cm\n/X");
                    content.write(characterName.c_str(), characterName.size());
                    content.writeText(" Do\n");
                }
            }
            charProcs->insertRef(characterName, doc->emit(SkPDFStream(content.detachAsStream())));
        }
        encDiffs->appendName(std::move(characterName));
        widthArray->appendScalar(advance);
    }

    if (!imageGlyphs.empty()) {
        auto d0 = sk_make_sp<SkPDFDict>();
        for (const auto& pair : imageGlyphs) {
            d0->insertRef(SkStringPrintf("Xg%X", pair.first), pair.second);
        }
        auto d1 = sk_make_sp<SkPDFDict>();
        d1->insertObject("XObject", std::move(d0));
        font.insertObject("Resources", std::move(d1));
    }

    encoding->insertObject("Differences", std::move(encDiffs));
    font.insertInt("FirstChar", 0);
    font.insertInt("LastChar", lastGlyphID - firstGlyphID + 1);
    /* FontBBox: "A rectangle expressed in the glyph coordinate
      system, specifying the font bounding box. This is the smallest
      rectangle enclosing the shape that would result if all of the
      glyphs of the font were placed with their origins coincident and
      then filled." */
    font.insertObject("FontBBox", SkPDFMakeArray(bbox.left(),
                                                  bbox.bottom(),
                                                  bbox.right(),
                                                  bbox.top()));

    font.insertName("CIDToGIDMap", "Identity");

    const std::vector<SkUnichar>& glyphToUnicode = SkPDFFont::GetUnicodeMap(typeface, canon);
    SkASSERT(glyphToUnicode.size() == SkToSizeT(typeface->countGlyphs()));
    sk_sp<SkPDFStream> toUnicodeCmap = SkPDFMakeToUnicodeCmap(glyphToUnicode.data(),
                                                              &subset,
                                                              false,
                                                              firstGlyphID,
                                                              lastGlyphID);
    font.insertRef("ToUnicode", doc->serialize(toUnicodeCmap));
    font.insertRef("FontDescriptor", type3_descriptor(doc, typeface, cache.get()));
    font.insertObject("Widths", std::move(widthArray));
    font.insertObject("Encoding", std::move(encoding));
    font.insertObject("CharProcs", std::move(charProcs));

    SkWStream* stream = doc->beginObject(pdfFont.indirectReference());
    font.emitObject(stream);
    doc->endObject();
}


void SkPDFFont::emitSubset(SkPDFDocument* doc) const {
    SkASSERT(fFontType != SkPDFFont().fFontType); // not default value
    switch (fFontType) {
        case SkAdvancedTypefaceMetrics::kType1CID_Font:
        case SkAdvancedTypefaceMetrics::kTrueType_Font:
            return emit_subset_type0(*this, doc);
        case SkAdvancedTypefaceMetrics::kType1_Font:
            return emit_subset_type1(*this, doc);
        default:
            return emit_subset_type3(*this, doc);
    }
}

////////////////////////////////////////////////////////////////////////////////

bool SkPDFFont::CanEmbedTypeface(SkTypeface* typeface, SkPDFCanon* canon) {
    const SkAdvancedTypefaceMetrics* metrics = SkPDFFont::GetMetrics(typeface, canon);
    return metrics && can_embed(*metrics);
}

