#pragma once
#include <cstdint>

namespace font {

// 5x7 monospace font (printable ASCII 0x20-0x7E).
// 1グリフ=5バイト（5列）。各バイトは縦8ピクセルで bit 0=最上段, bit 6=7段目（bit 7 未使用）。
constexpr int CHAR_W   = 5;
constexpr int CHAR_H   = 7;
constexpr int SPACING  = 1;
constexpr int ADVANCE  = CHAR_W + SPACING;  // 6 px / char

// 5バイトのコラムデータを返す。範囲外は空白扱い。
const uint8_t* glyph(char c);

// 8x8 ひらがな（美咲ゴシック）。kana.h の index で引く。
// 1 グリフ = 8 バイト（1 行 1 バイト、bit 7 が左端、先頭バイトが最上段）。
// 実際の字は 7x7 に収まっていて、右列・下段が字間・行間になる。
constexpr int KANA_W       = 8;
constexpr int KANA_H       = 8;
constexpr int KANA_ADVANCE = 8;   // 字送り。字形側に余白があるので詰めない

// index が範囲外（kana::END を含む）のときは空グリフを返す。
const uint8_t* kanaGlyph(uint8_t index);

}  // namespace font
