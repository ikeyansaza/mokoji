#pragma once
#include <cstdint>
#include "hardware/i2c.h"

// 128x64 SSD1306 OLED ドライバ（I2C 接続）の最小実装。
// フレームバッファを RAM に持ち、show() で OLED に転送する。
class SSD1306 {
public:
    static constexpr int W = 128;
    static constexpr int H = 64;
    static constexpr int PAGES = H / 8;
    static constexpr int BUF_SIZE = W * PAGES;

    SSD1306(i2c_inst_t* i2c, uint8_t addr = 0x3C);

    void begin();                  // 初期化シーケンス送信
    void clear();                  // バッファ全消し（≒ MicroPython の fill(0)）
    void setPixel(int x, int y, bool on);
    void fillRect(int x, int y, int w, int h, bool on);
    // 汎用スプライト描画。row_bytes は (w + 7) / 8。
    void drawSpriteRaw(const uint8_t* sprite, int w, int h, int row_bytes, int x, int y);
    // 後方互換用：24x24
    void drawSprite(const uint8_t sprite[24][3], int x, int y);
    // 2 倍スケール描画：1 ピクセルを 2x2 ブロックとして描く
    void drawSprite2x(const uint8_t sprite[24][3], int x, int y);
    // UTF-8 文字列を描く。ASCII は 5x7（字送り 6px）、それ以外は 8x8 の日本語（字送り 8px、
    // ひらがな・カタカナ・記号・JIS 第一水準漢字）。未収録字は空白 8px。混在可、上端揃え。
    // inverse のときは字形を反転し、各文字のセル（字間・行間を含む）を塗りつぶす。
    // scale は整数倍率（2 なら ASCII 10x14、日本語 16x16）。字送りも scale 倍になる。
    void drawText(const char* s, int x, int y, bool inverse = false, int scale = 1);
    // ひらがな 8x8。kana index 列（kana::END 終端、最大 kana::MAX_NAME 字）を左から並べる。
    // inverse のときは字形を反転し、8x8 セル全体（字間・行間も）を塗りつぶす。
    // scale は整数倍率（2 なら 16x16）。字送りも scale 倍になる。
    void drawKana(const uint8_t* kana_indices, int x, int y, bool inverse = false, int scale = 1);
    void show();                   // バッファを OLED に転送

private:
    // 8x8 グリフ 1 字（1 行 1 バイト、bit7 = 左端）を scale 倍で描く。glyph が nullptr なら空白セル。
    void drawGlyph8(const uint8_t* glyph, int x, int y, bool inverse, int scale);
    i2c_inst_t* _i2c;
    uint8_t _addr;
    uint8_t _buf[BUF_SIZE];

    void cmd(uint8_t c);
    void cmdList(const uint8_t* cmds, int n);
};
