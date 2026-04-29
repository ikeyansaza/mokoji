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

}  // namespace font
