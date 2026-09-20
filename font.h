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

// ---- UTF-8 テキスト --------------------------------------------------------
// ASCII は 5x7（ADVANCE = 6px）、それ以外は 8x8 の日本語グリフ（KANA_ADVANCE = 8px）で描く。
// 収録字：ひらがな・カタカナ・記号・JIS 第一水準漢字（ja_font.h、kana_font_gen.py が生成）。

constexpr uint32_t REPLACEMENT = 0xFFFD;   // 不正なバイト列の代用コードポイント

// s が指す 1 文字を UTF-8 として読み、コードポイントを返して s を進める。
// 不正・途中で切れた列は REPLACEMENT を返す。終端 NUL を越えては読まず、
// NUL 以外では必ず 1 バイト以上進む（呼び出し側の while ループが止まる）。
// *s が NUL のときは 0 を返し、進めない。
uint32_t decodeUtf8(const char*& s);

// cp（>= 0x80）の 8x8 グリフを返す。未収録・ASCII・BMP 外は nullptr。
// 1 グリフ = 8 バイト（1 行 1 バイト、bit 7 が左端）。
const uint8_t* jaGlyph(uint32_t cp);

// UTF-8 文字列の描画幅（px）。ASCII は ADVANCE、それ以外（不正バイト含む）は KANA_ADVANCE。
int textWidth(const char* utf8);

}  // namespace font
