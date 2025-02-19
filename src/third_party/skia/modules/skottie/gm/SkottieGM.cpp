/*
 * Copyright 2017 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gm.h"
#include "Resources.h"
#include "SkAnimCodecPlayer.h"
#include "SkAnimTimer.h"
#include "SkColor.h"
#include "SkMakeUnique.h"
#include "Skottie.h"
#include "SkottieProperty.h"
#include "SkottieUtils.h"

#include <cmath>
#include <vector>

using namespace skottie;

namespace {

static constexpr char kWebFontResource[] = "fonts/Roboto-Regular.ttf";
static constexpr char kSkottieResource[] = "skottie/skottie_sample_webfont.json";

// Dummy web font loader which serves a single local font (checked in under resources/).
class FakeWebFontProvider final : public ResourceProvider {
public:
    FakeWebFontProvider() : fFontData(GetResourceAsData(kWebFontResource)) {}

    sk_sp<SkData> loadFont(const char[], const char[]) const override {
        return fFontData;
    }

private:
    sk_sp<SkData> fFontData;

    using INHERITED = ResourceProvider;
};

} // namespace

class SkottieWebFontGM : public skiagm::GM {
public:
protected:
    SkString onShortName() override {
        return SkString("skottie_webfont");
    }

    SkISize onISize() override {
        return SkISize::Make(kSize, kSize);
    }

    void onOnceBeforeDraw() override {
        if (auto stream = GetResourceAsStream(kSkottieResource)) {
            fAnimation = Animation::Builder()
                            .setResourceProvider(sk_make_sp<FakeWebFontProvider>())
                            .make(stream.get());
        }
    }

    void onDraw(SkCanvas* canvas) override {
        if (!fAnimation) {
            return;
        }

        auto dest = SkRect::MakeWH(kSize, kSize);
        fAnimation->render(canvas, &dest);
    }

    bool onAnimate(const SkAnimTimer& timer) override {
        if (!fAnimation) {
            return false;
        }

        const auto duration = fAnimation->duration();
        fAnimation->seek(std::fmod(timer.secs(), duration) / duration);
        return true;
    }

private:
    static constexpr SkScalar kSize = 800;

    sk_sp<Animation> fAnimation;

    using INHERITED = skiagm::GM;
};

DEF_GM(return new SkottieWebFontGM;)

using namespace skottie_utils;

class SkottieColorizeGM : public skiagm::GM {
protected:
    SkString onShortName() override {
        return SkString("skottie_colorize");
    }

    SkISize onISize() override {
        return SkISize::Make(kSize, kSize);
    }

    void onOnceBeforeDraw() override {
        if (auto stream = GetResourceAsStream("skottie/skottie_sample_search.json")) {
            auto propBuilder = sk_make_sp<CustomPropertyManagerBuilder>();
            fAnimation   = Animation::Builder()
                              .setPropertyObserver(propBuilder)
                              .make(stream.get());
            fPropManager = propBuilder->build();
            fColors      = fPropManager->getColorProps();
        }
    }

    void onDraw(SkCanvas* canvas) override {
        if (!fAnimation) {
            return;
        }

        auto dest = SkRect::MakeWH(kSize, kSize);
        fAnimation->render(canvas, &dest);
    }

    bool onAnimate(const SkAnimTimer& timer) override {
        if (!fAnimation) {
            return false;
        }

        const auto duration = fAnimation->duration();
        fAnimation->seek(std::fmod(timer.secs(), duration) / duration);
        return true;
    }

    bool onHandleKey(SkUnichar uni) override {
        static constexpr SkColor kColors[] = {
            SK_ColorBLACK,
            SK_ColorRED,
            SK_ColorGREEN,
            SK_ColorYELLOW,
            SK_ColorCYAN,
        };

        if (uni == 'c') {
            fColorIndex = (fColorIndex + 1) % SK_ARRAY_COUNT(kColors);
            for (const auto& prop : fColors) {
                fPropManager->setColor(prop, kColors[fColorIndex]);
            }
            return true;
        }

        return false;
    }

private:
    static constexpr SkScalar kSize = 800;

    sk_sp<Animation>                            fAnimation;
    std::unique_ptr<CustomPropertyManager>      fPropManager;
    std::vector<CustomPropertyManager::PropKey> fColors;
    size_t                                      fColorIndex = 0;

    using INHERITED = skiagm::GM;
};

DEF_GM(return new SkottieColorizeGM;)

class SkottieMultiFrameGM : public skiagm::GM {
public:
protected:
    SkString onShortName() override {
        return SkString("skottie_multiframe");
    }

    SkISize onISize() override {
        return SkISize::Make(kSize, kSize);
    }

    void onOnceBeforeDraw() override {
        if (auto stream = GetResourceAsStream("skottie/skottie_sample_multiframe.json")) {
            fAnimation = Animation::Builder()
                            .setResourceProvider(sk_make_sp<MultiFrameResourceProvider>())
                            .make(stream.get());
        }
    }

    void onDraw(SkCanvas* canvas) override {
        if (!fAnimation) {
            return;
        }

        auto dest = SkRect::MakeWH(kSize, kSize);
        fAnimation->render(canvas, &dest);
    }

    bool onAnimate(const SkAnimTimer& timer) override {
        if (!fAnimation) {
            return false;
        }

        const auto duration = fAnimation->duration();
        fAnimation->seek(std::fmod(timer.secs(), duration) / duration);
        return true;
    }

private:
    class MultiFrameResourceProvider final : public skottie::ResourceProvider {
    public:
        sk_sp<ImageAsset> loadImageAsset(const char[], const char[]) const override {
            return skottie_utils::MultiFrameImageAsset::Make(
                        GetResourceAsData("images/flightAnim.gif"));
        }
    };

    static constexpr SkScalar kSize = 800;

    sk_sp<Animation> fAnimation;

    using INHERITED = skiagm::GM;
};

DEF_GM(return new SkottieMultiFrameGM;)
