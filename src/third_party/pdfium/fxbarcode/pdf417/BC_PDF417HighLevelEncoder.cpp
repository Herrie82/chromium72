// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com
// Original code is licensed as follows:
/*
 * Copyright 2006 Jeremias Maerki in part, and ZXing Authors in part
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "fxbarcode/pdf417/BC_PDF417HighLevelEncoder.h"

#include "core/fxcrt/fx_extension.h"
#include "third_party/bigint/BigIntegerLibrary.hh"

namespace {

constexpr int32_t kLatchToText = 900;
constexpr int32_t kLatchToBytePadded = 901;
constexpr int32_t kLatchToNumeric = 902;
constexpr int32_t kShiftToByte = 913;
constexpr int32_t kLatchToByte = 924;

constexpr uint8_t kTextMixedRaw[] = {48, 49, 50, 51, 52, 53, 54, 55, 56, 57,
                                     38, 13, 9,  44, 58, 35, 45, 46, 36, 47,
                                     43, 37, 42, 61, 94, 0,  32, 0,  0,  0};
constexpr uint8_t kTextPunctuationRaw[] = {
    59, 60, 62, 64, 91, 92, 93,  95, 96, 126, 33, 13,  9,   44, 58,
    10, 45, 46, 36, 47, 34, 124, 42, 40, 41,  63, 123, 125, 39, 0};

int32_t g_mixed[128] = {0};
int32_t g_punctuation[128] = {0};

bool IsAlphaUpperOrSpace(wchar_t ch) {
  return ch == ' ' || (ch >= 'A' && ch <= 'Z');
}

bool IsAlphaLowerOrSpace(wchar_t ch) {
  return ch == ' ' || (ch >= 'a' && ch <= 'z');
}

bool IsMixed(wchar_t ch) {
  // Bounds check avoiding sign mismatch error given questionable signedness.
  return !((ch & ~0x7F) || g_mixed[ch] == -1);
}

bool IsPunctuation(wchar_t ch) {
  // Bounds check avoiding sign mismatch error given questionable signedness.
  return !((ch & ~0x7F) || g_punctuation[ch] == -1);
}

bool IsText(wchar_t ch) {
  return (ch >= 32 && ch <= 126) || ch == '\t' || ch == '\n' || ch == '\r';
}

void Inverse() {
  for (size_t l = 0; l < FX_ArraySize(g_mixed); ++l)
    g_mixed[l] = -1;

  for (uint8_t i = 0; i < FX_ArraySize(kTextMixedRaw); ++i) {
    uint8_t b = kTextMixedRaw[i];
    if (b != 0)
      g_mixed[b] = i;
  }

  for (size_t l = 0; l < FX_ArraySize(g_punctuation); ++l)
    g_punctuation[l] = -1;

  for (uint8_t i = 0; i < FX_ArraySize(kTextPunctuationRaw); ++i) {
    uint8_t b = kTextPunctuationRaw[i];
    if (b != 0)
      g_punctuation[b] = i;
  }
}

}  // namespace

void CBC_PDF417HighLevelEncoder::Initialize() {
  Inverse();
}

void CBC_PDF417HighLevelEncoder::Finalize() {}

Optional<WideString> CBC_PDF417HighLevelEncoder::EncodeHighLevel(
    const WideString& msg) {
  ByteString bytes = msg.ToUTF8();
  size_t len = bytes.GetLength();
  WideString result;
  result.Reserve(len);
  for (size_t i = 0; i < len; i++) {
    wchar_t ch = bytes[i] & 0xff;
    if (ch == '?' && bytes[i] != '?')
      return {};

    result += ch;
  }
  std::vector<uint8_t> byteArr(bytes.begin(), bytes.end());
  len = result.GetLength();
  WideString sb;
  sb.Reserve(len);
  size_t p = 0;
  SubMode textSubMode = SubMode::kAlpha;
  EncodingMode encodingMode = EncodingMode::kUnknown;
  while (p < len) {
    size_t n = DetermineConsecutiveDigitCount(result, p);
    if (n >= 13) {
      sb += kLatchToNumeric;
      encodingMode = EncodingMode::kNumeric;
      textSubMode = SubMode::kAlpha;
      EncodeNumeric(result, p, n, &sb);
      p += n;
    } else {
      size_t t = DetermineConsecutiveTextCount(result, p);
      if (t >= 5 || n == len) {
        if (encodingMode != EncodingMode::kText) {
          sb += kLatchToText;
          encodingMode = EncodingMode::kText;
          textSubMode = SubMode::kAlpha;
        }
        textSubMode = EncodeText(result, p, t, textSubMode, &sb);
        p += t;
      } else {
        Optional<size_t> b =
            DetermineConsecutiveBinaryCount(result, &byteArr, p);
        if (!b)
          return {};

        size_t b_value = b.value();
        if (b_value == 0) {
          b_value = 1;
        }
        if (b_value == 1 && encodingMode == EncodingMode::kText) {
          EncodeBinary(byteArr, p, 1, EncodingMode::kText, &sb);
        } else {
          EncodeBinary(byteArr, p, b_value, encodingMode, &sb);
          encodingMode = EncodingMode::kByte;
          textSubMode = SubMode::kAlpha;
        }
        p += b_value;
      }
    }
  }
  return sb;
}

CBC_PDF417HighLevelEncoder::SubMode CBC_PDF417HighLevelEncoder::EncodeText(
    const WideString& msg,
    size_t startpos,
    size_t count,
    SubMode initialSubmode,
    WideString* sb) {
  WideString tmp;
  tmp.Reserve(count);
  SubMode submode = initialSubmode;
  size_t idx = 0;
  while (idx < count) {
    wchar_t ch = msg[startpos + idx];
    switch (submode) {
      case SubMode::kAlpha:
        if (IsAlphaUpperOrSpace(ch)) {
          if (ch == ' ')
            tmp += 26;
          else
            tmp += ch - 65;
          break;
        }
        if (IsAlphaLowerOrSpace(ch)) {
          submode = SubMode::kLower;
          tmp += 27;
          continue;
        }
        if (IsMixed(ch)) {
          submode = SubMode::kMixed;
          tmp += 28;
          continue;
        }
        if (IsPunctuation(ch)) {
          tmp += 29;
          tmp += g_punctuation[ch];
        }
        break;
      case SubMode::kLower:
        if (IsAlphaLowerOrSpace(ch)) {
          if (ch == ' ')
            tmp += 26;
          else
            tmp += ch - 97;
          break;
        }
        if (IsAlphaUpperOrSpace(ch)) {
          tmp += 27;
          tmp += ch - 65;
          break;
        }
        if (IsMixed(ch)) {
          submode = SubMode::kMixed;
          tmp += 28;
          continue;
        }
        if (IsPunctuation(ch)) {
          tmp += 29;
          tmp += g_punctuation[ch];
        }
        break;
      case SubMode::kMixed:
        if (IsMixed(ch)) {
          tmp += g_mixed[ch];
          break;
        }
        if (IsAlphaUpperOrSpace(ch)) {
          submode = SubMode::kAlpha;
          tmp += 28;
          continue;
        }
        if (IsAlphaLowerOrSpace(ch)) {
          submode = SubMode::kLower;
          tmp += 27;
          continue;
        }
        if (startpos + idx + 1 < count) {
          wchar_t next = msg[startpos + idx + 1];
          if (IsPunctuation(next)) {
            submode = SubMode::kPunctuation;
            tmp += 25;
            continue;
          }
        }
        if (IsPunctuation(ch)) {
          tmp += 29;
          tmp += g_punctuation[ch];
        }
        break;
      default:
        if (IsPunctuation(ch)) {
          tmp += g_punctuation[ch];
          break;
        }
        submode = SubMode::kAlpha;
        tmp += 29;
        continue;
    }
    ++idx;
  }
  wchar_t h = 0;
  size_t len = tmp.GetLength();
  for (size_t i = 0; i < len; i++) {
    bool odd = (i % 2) != 0;
    if (odd) {
      h = (h * 30) + tmp[i];
      *sb += h;
    } else {
      h = tmp[i];
    }
  }
  if ((len % 2) != 0)
    *sb += (h * 30) + 29;
  return submode;
}

void CBC_PDF417HighLevelEncoder::EncodeBinary(const std::vector<uint8_t>& bytes,
                                              size_t startpos,
                                              size_t count,
                                              EncodingMode startmode,
                                              WideString* sb) {
  if (count == 1 && startmode == EncodingMode::kText)
    *sb += kShiftToByte;

  size_t idx = startpos;
  if (count >= 6) {
    *sb += kLatchToByte;
    wchar_t chars[5];
    while ((startpos + count - idx) >= 6) {
      int64_t t = 0;
      for (size_t i = 0; i < 6; i++) {
        t <<= 8;
        t += bytes[idx + i] & 0xff;
      }
      for (size_t i = 0; i < 5; i++) {
        chars[i] = (t % 900);
        t /= 900;
      }
      for (size_t i = 5; i >= 1; i--)
        *sb += (chars[i - 1]);
      idx += 6;
    }
  }
  if (idx < startpos + count)
    *sb += kLatchToBytePadded;
  for (size_t i = idx; i < startpos + count; i++) {
    int32_t ch = bytes[i] & 0xff;
    *sb += ch;
  }
}

void CBC_PDF417HighLevelEncoder::EncodeNumeric(const WideString& msg,
                                               size_t startpos,
                                               size_t count,
                                               WideString* sb) {
  size_t idx = 0;
  BigInteger num900 = 900;
  while (idx < count) {
    WideString tmp;
    size_t len = 44 < count - idx ? 44 : count - idx;
    ByteString part = (L'1' + msg.Mid(startpos + idx, len)).ToUTF8();
    BigInteger bigint = stringToBigInteger(part.c_str());
    do {
      int32_t c = (bigint % num900).toInt();
      tmp += c;
      bigint = bigint / num900;
    } while (!bigint.isZero());
    for (size_t i = tmp.GetLength(); i >= 1; i--)
      *sb += tmp[i - 1];
    idx += len;
  }
}

size_t CBC_PDF417HighLevelEncoder::DetermineConsecutiveDigitCount(
    WideString msg,
    size_t startpos) {
  size_t count = 0;
  size_t len = msg.GetLength();
  size_t idx = startpos;
  if (idx < len) {
    wchar_t ch = msg[idx];
    while (FXSYS_IsDecimalDigit(ch) && idx < len) {
      count++;
      idx++;
      if (idx < len)
        ch = msg[idx];
    }
  }
  return count;
}

size_t CBC_PDF417HighLevelEncoder::DetermineConsecutiveTextCount(
    WideString msg,
    size_t startpos) {
  size_t len = msg.GetLength();
  size_t idx = startpos;
  while (idx < len) {
    wchar_t ch = msg[idx];
    size_t numericCount = 0;
    while (numericCount < 13 && FXSYS_IsDecimalDigit(ch) && idx < len) {
      numericCount++;
      idx++;
      if (idx < len)
        ch = msg[idx];
    }
    if (numericCount >= 13)
      return idx - startpos - numericCount;
    if (numericCount > 0)
      continue;
    ch = msg[idx];
    if (!IsText(ch))
      break;
    idx++;
  }
  return idx - startpos;
}

Optional<size_t> CBC_PDF417HighLevelEncoder::DetermineConsecutiveBinaryCount(
    WideString msg,
    std::vector<uint8_t>* bytes,
    size_t startpos) {
  size_t len = msg.GetLength();
  size_t idx = startpos;
  while (idx < len) {
    wchar_t ch = msg[idx];
    size_t numericCount = 0;
    while (numericCount < 13 && FXSYS_IsDecimalDigit(ch)) {
      numericCount++;
      size_t i = idx + numericCount;
      if (i >= len)
        break;
      ch = msg[i];
    }
    if (numericCount >= 13)
      return idx - startpos;

    size_t textCount = 0;
    while (textCount < 5 && IsText(ch)) {
      textCount++;
      size_t i = idx + textCount;
      if (i >= len)
        break;
      ch = msg[i];
    }
    if (textCount >= 5)
      return idx - startpos;
    ch = msg[idx];
    if ((*bytes)[idx] == 63 && ch != '?')
      return {};
    idx++;
  }
  return idx - startpos;
}
