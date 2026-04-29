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
    void drawSprite(const uint8_t sprite[24][3], int x, int y);
    void drawText(const char* s, int x, int y, bool inverse = false);
    void show();                   // バッファを OLED に転送

private:
    i2c_inst_t* _i2c;
    uint8_t _addr;
    uint8_t _buf[BUF_SIZE];

    void cmd(uint8_t c);
    void cmdList(const uint8_t* cmds, int n);
};
