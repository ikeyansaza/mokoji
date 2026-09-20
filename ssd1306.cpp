#include "ssd1306.h"
#include "font.h"
#include "kana.h"
#include <cstring>

namespace {
// I2C コントロールバイト：D/C=0 → コマンド、D/C=1 → データ
constexpr uint8_t CTRL_CMD  = 0x00;
constexpr uint8_t CTRL_DATA = 0x40;

// 128x64 用初期化シーケンス
constexpr uint8_t kInit[] = {
    CTRL_CMD,
    0xAE,             // Display OFF
    0xD5, 0x80,       // Clock div / oscillator
    0xA8, 0x3F,       // Multiplex 64
    0xD3, 0x00,       // Display offset 0
    0x40,             // Start line 0
    0x8D, 0x14,       // Charge pump ON
    0x20, 0x00,       // Memory mode = horizontal
    0xA1,             // Segment remap (left-right normal)
    0xC8,             // COM scan direction reversed
    0xDA, 0x12,       // COM pins config (128x64)
    0x81, 0xCF,       // Contrast
    0xD9, 0xF1,       // Pre-charge period
    0xDB, 0x40,       // VCOMH deselect
    0xA4,             // Display follows RAM
    0xA6,             // Normal display (not inverted)
    0xAF,             // Display ON
};
}  // namespace

SSD1306::SSD1306(i2c_inst_t* i2c, uint8_t addr) : _i2c(i2c), _addr(addr) {
    memset(_buf, 0, sizeof(_buf));
}

void SSD1306::cmd(uint8_t c) {
    uint8_t b[2] = { CTRL_CMD, c };
    i2c_write_blocking(_i2c, _addr, b, 2, false);
}

void SSD1306::cmdList(const uint8_t* cmds, int n) {
    i2c_write_blocking(_i2c, _addr, cmds, n, false);
}

void SSD1306::begin() {
    cmdList(kInit, sizeof(kInit));
}

void SSD1306::clear() {
    memset(_buf, 0, sizeof(_buf));
}

void SSD1306::setPixel(int x, int y, bool on) {
    if (x < 0 || x >= W || y < 0 || y >= H) return;
    int page = y >> 3;
    int bit  = y & 7;
    uint8_t mask = uint8_t(1u << bit);
    if (on)  _buf[page * W + x] |=  mask;
    else     _buf[page * W + x] &= ~mask;
}

void SSD1306::fillRect(int x, int y, int w, int h, bool on) {
    for (int dy = 0; dy < h; ++dy)
        for (int dx = 0; dx < w; ++dx)
            setPixel(x + dx, y + dy, on);
}

void SSD1306::drawSpriteRaw(const uint8_t* sprite, int w, int h, int row_bytes, int x, int y) {
    for (int row = 0; row < h; ++row) {
        for (int col = 0; col < w; ++col) {
            int bi  = col >> 3;
            int bit = 7 - (col & 7);
            if ((sprite[row * row_bytes + bi] >> bit) & 1) {
                setPixel(x + col, y + row, true);
            }
        }
    }
}

void SSD1306::drawSprite(const uint8_t sprite[24][3], int x, int y) {
    drawSpriteRaw(reinterpret_cast<const uint8_t*>(sprite), 24, 24, 3, x, y);
}

void SSD1306::drawSpriteRotCW(const uint8_t sprite[24][3], int x, int y) {
    // 元の (row, col) は、時計回り 90° で (x + 23 - row, y + col) に移る。
    for (int row = 0; row < 24; ++row) {
        for (int col = 0; col < 24; ++col) {
            if ((sprite[row][col >> 3] >> (7 - (col & 7))) & 1) {
                setPixel(x + (23 - row), y + col, true);
            }
        }
    }
}

void SSD1306::drawSprite2x(const uint8_t sprite[24][3], int x, int y) {
    // 24x24 ソースを 48x48 出力で描画。各ピクセル = 2x2 ブロック。
    for (int row = 0; row < 24; ++row) {
        for (int col = 0; col < 24; ++col) {
            int bi  = col >> 3;
            int bit = 7 - (col & 7);
            if ((sprite[row][bi] >> bit) & 1) {
                fillRect(x + col * 2, y + row * 2, 2, 2, true);
            }
        }
    }
}

void SSD1306::drawGlyph8(const uint8_t* glyph, int x, int y, bool inverse, int scale) {
    for (int row = 0; row < font::KANA_H; ++row) {
        for (int col = 0; col < font::KANA_W; ++col) {
            bool on = glyph && ((glyph[row] >> (7 - col)) & 1);
            if (inverse) on = !on;
            fillRect(x + col * scale, y + row * scale, scale, scale, on);
        }
    }
}

void SSD1306::drawText(const char* s, int x, int y, bool inverse, int scale) {
    while (*s) {
        const uint32_t cp = font::decodeUtf8(s);
        if (cp < 0x80) {
            const uint8_t* g = font::glyph(char(cp));
            for (int col = 0; col < font::CHAR_W; ++col) {
                uint8_t bits = g[col];
                for (int row = 0; row < font::CHAR_H; ++row) {
                    bool on = (bits >> row) & 1;
                    if (inverse) on = !on;
                    fillRect(x + col * scale, y + row * scale, scale, scale, on);
                }
                // spacing column
                if (inverse) {
                    fillRect(x + font::CHAR_W * scale, y, scale, font::CHAR_H * scale, true);
                }
            }
            x += font::ADVANCE * scale;
        } else {
            drawGlyph8(font::jaGlyph(cp), x, y, inverse, scale);
            x += font::KANA_ADVANCE * scale;
        }
    }
}

void SSD1306::drawKana(const uint8_t* kana_indices, int x, int y, bool inverse, int scale) {
    for (int n = 0; n < kana::MAX_NAME && kana_indices[n] != kana::END; ++n) {
        drawGlyph8(font::kanaGlyph(kana_indices[n]), x, y, inverse, scale);
        x += font::KANA_ADVANCE * scale;
    }
}

void SSD1306::show() {
    // アドレス窓を全画面に設定
    static constexpr uint8_t addrCmds[] = {
        CTRL_CMD,
        0x21, 0x00, 0x7F,  // column 0..127
        0x22, 0x00, 0x07,  // page   0..7
    };
    cmdList(addrCmds, sizeof(addrCmds));

    // フレームバッファをデータとして送信。ヘッダ 0x40 を頭に付けるためにテンポラリへコピー。
    static uint8_t tx[BUF_SIZE + 1];
    tx[0] = CTRL_DATA;
    memcpy(tx + 1, _buf, BUF_SIZE);
    i2c_write_blocking(_i2c, _addr, tx, BUF_SIZE + 1, false);
}
