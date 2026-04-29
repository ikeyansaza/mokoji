#include "display.h"
#include "sprites.h"
#include "kana.h"
#include "pico/rand.h"
#include <cstdio>
#include <cstring>

Display::Display(SSD1306* oled) : _oled(oled) {}

void Display::draw(const Game& g) {
    _oled->clear();

    if (g.sleeping()) {
        drawSleep(g);
        return;
    }
    switch (g.screen()) {
        case Game::Screen::MAIN:   drawMain(g);   break;
        case Game::Screen::MENU:   drawMenu(g);   break;
        case Game::Screen::GRAVE:  drawGrave(g);  break;
        case Game::Screen::NAMING: drawNaming(g); break;
    }
}

void Display::drawMain(const Game& g) {
    // 上段ステータス：空腹バー + 名前
    int hbars = g.hunger() / 20;
    char status[16];
    int p = 0;
    status[p++] = 'H';
    status[p++] = ':';
    for (int i = 0; i < hbars; ++i)        status[p++] = '|';
    for (int i = hbars; i < 5; ++i)        status[p++] = '.';
    status[p] = '\0';
    _oled->drawText(status, 0, 0);
    _oled->drawText(g.name(), 96, 0);

    // 羊本体
    auto sprite = selectSprite(g, g.face());
    _oled->drawSprite(sprite, g.walkX(), 20);

    if (g.wool() > 30) drawWool(g.wool(), g.walkX());

    _oled->drawText("< menu >", 40, 56);
}

void Display::drawMenu(const Game& g) {
    int cursor = g.menuCursor();
    for (int i = 0; i < Game::MENU_COUNT; ++i) {
        int x = i * 32;
        if (i == cursor) {
            _oled->fillRect(x, 52, 30, 12, true);
            _oled->drawText(Game::menuLabel(i), x + 1, 54, true);
        } else {
            _oled->drawText(Game::menuLabel(i), x + 1, 54, false);
        }
    }
    auto sprite = selectSprite(g, Game::Face::FRONT);
    _oled->drawSprite(sprite, 52, 20);
}

void Display::drawSleep(const Game& g) {
    auto sprite = selectSprite(g, Game::Face::FRONT);
    _oled->drawSprite(sprite, 52, 20);
    _oled->drawText("zzz...", 56, 48);
}

void Display::drawGrave(const Game& g) {
    _oled->drawText("+ MEMORIES +", 0, 0);
    int n = g.graveCount();
    // 新しい順に最大 3 件を表示（i=0 が最新）
    int show = (n > 3) ? 3 : n;
    for (int i = 0; i < show; ++i) {
        const GraveRecord& gr = g.grave(n - 1 - i);
        char line[40];
        // TODO Phase 2: ひらがなフォント実装後に「天寿をまっとう」「旅立ち」へ。
        // 暫定 ASCII: '*' = 天寿（age）、'x' = 餓死/不幸死（status）
        char tag = (gr.death_cause == uint8_t(Game::DeathCause::AGE)) ? '*' : 'x';
        std::snprintf(line, sizeof(line), "%-4s %-4s %dd %c",
                      gr.name, gr.breed, int(gr.age_days), tag);
        _oled->drawText(line, 0, 16 + i * 12);
    }
    _oled->drawText("press btn", 0, 56);
}

const uint8_t (*Display::selectSprite(const Game& g, Game::Face face))[3] {
    using namespace sprites;

    const uint8_t (*L)[3] = LAMB_L;
    const uint8_t (*F)[3] = LAMB_F;
    const uint8_t (*R)[3] = LAMB_R;

    switch (g.stage()) {
        case Game::Stage::LAMB:
            L = LAMB_L; F = LAMB_F; R = LAMB_R; break;
        case Game::Stage::YOUNG_MOKO:
            L = YOUNG_MOKO_L; F = YOUNG_MOKO_F; R = YOUNG_MOKO_R; break;
        case Game::Stage::YOUNG_SURA:
            L = YOUNG_SURA_L; F = YOUNG_SURA_F; R = YOUNG_SURA_R; break;
        case Game::Stage::YOUNG_RARE:
            L = YOUNG_RARE_L; F = YOUNG_RARE_F; R = YOUNG_RARE_R; break;
        case Game::Stage::ADULT:
            switch (g.breed()) {
                case Game::Breed::CORRIEDALE:
                    L = ADULT_COR_L;  F = ADULT_COR_F;  R = ADULT_COR_R;  break;
                case Game::Breed::MERINO:
                    L = ADULT_MER_L;  F = ADULT_MER_F;  R = ADULT_MER_R;  break;
                case Game::Breed::SUFFOLK:
                    L = ADULT_SUF_L;  F = ADULT_SUF_F;  R = ADULT_SUF_R;  break;
                case Game::Breed::SOUTHDOWN:
                    L = ADULT_SOU_L;  F = ADULT_SOU_F;  R = ADULT_SOU_R;  break;
                case Game::Breed::EASTFRIESIAN:
                    L = ADULT_EAST_L; F = ADULT_EAST_F; R = ADULT_EAST_R; break;
                default: break;
            }
            break;
    }

    switch (face) {
        case Game::Face::LEFT:  return L;
        case Game::Face::RIGHT: return R;
        case Game::Face::FRONT: return F;
    }
    return F;
}

// =====================================================================
// 命名画面：プリセット選択 / 手動入力
// 注意：当面はかな名を romaji で render する（ASCII 5x7 フォントのみ実装）。
// ひらがなフォントを後で実装したら、ここの drawText を hiragana 描画に
// 差し替えれば良い。
// =====================================================================
void Display::drawNaming(const Game& g) {
    char buf[32];
    switch (g.namingMode()) {
        case Game::NamingMode::SELECT_MODE: {
            _oled->drawText("name?", 0, 0);
            int cur = g.namingCursor();
            // 上：preset
            if (cur == 0) {
                _oled->fillRect(0, 18, 80, 12, true);
                _oled->drawText("preset", 4, 20, true);
            } else {
                _oled->drawText("preset", 4, 20, false);
            }
            // 下：type
            if (cur == 1) {
                _oled->fillRect(0, 36, 80, 12, true);
                _oled->drawText("type", 4, 38, true);
            } else {
                _oled->drawText("type", 4, 38, false);
            }
            _oled->drawText("L/R / OK", 0, 56);
            break;
        }
        case Game::NamingMode::PRESET_PICK: {
            _oled->drawText("preset", 0, 0);
            int idx = g.namingCursor();
            kana::toRomaji(kana::PRESETS[idx], buf, sizeof(buf));
            // 中央寄せで現在の preset 名を表示
            int width = std::strlen(buf) * 6;
            int x = (128 - width) / 2;
            if (x < 0) x = 0;
            _oled->drawText(buf, x, 28);
            std::snprintf(buf, sizeof(buf), "%d/%d", idx + 1, kana::PRESET_COUNT);
            _oled->drawText(buf, 0, 16);
            _oled->drawText("< L  R >  OK", 0, 56);
            break;
        }
        case Game::NamingMode::INPUT_ROW: {
            // 上段：現在の入力バッファ
            char nbuf[16];
            kana::toRomaji(g.inputBuffer(), nbuf, sizeof(nbuf));
            std::snprintf(buf, sizeof(buf), "[%s_]", nbuf);
            _oled->drawText(buf, 0, 0);
            // 中段：行を選ぶ
            int cur = g.namingCursor();
            const int total = kana::ROW_COUNT + 2;   // + BS + OK
            const char* label;
            if (cur < kana::ROW_COUNT)              label = kana::ROWS[cur].label;
            else if (cur == kana::ROW_COUNT)        label = "BS";
            else                                    label = "OK";
            int width = std::strlen(label) * 6;
            int x = (128 - width) / 2;
            if (x < 0) x = 0;
            _oled->fillRect(x - 2, 26, width + 4, 12, true);
            _oled->drawText(label, x, 28, true);
            std::snprintf(buf, sizeof(buf), "%d/%d", cur + 1, total);
            _oled->drawText(buf, 0, 44);
            _oled->drawText("< L  R >  OK", 0, 56);
            break;
        }
        case Game::NamingMode::INPUT_CHAR: {
            char nbuf[16];
            kana::toRomaji(g.inputBuffer(), nbuf, sizeof(nbuf));
            std::snprintf(buf, sizeof(buf), "[%s_]", nbuf);
            _oled->drawText(buf, 0, 0);
            const auto& row = kana::ROWS[g.inputRow()];
            int cur = g.namingCursor();
            uint8_t kanaIdx = uint8_t(row.start + cur);
            const char* romaji = (kanaIdx < kana::COUNT) ? kana::TABLE[kanaIdx].romaji : "?";
            int width = std::strlen(romaji) * 6;
            int x = (128 - width) / 2;
            if (x < 0) x = 0;
            _oled->fillRect(x - 2, 26, width + 4, 12, true);
            _oled->drawText(romaji, x, 28, true);
            std::snprintf(buf, sizeof(buf), "%s row %d/%d", row.label, cur + 1, row.length);
            _oled->drawText(buf, 0, 44);
            _oled->drawText("< L  R >  OK", 0, 56);
            break;
        }
    }
}

void Display::drawWool(int wool, int sx) {
    int density = wool / 10;
    int dots = density * 8;
    for (int i = 0; i < dots; ++i) {
        int dx = int(get_rand_32() & 0x1Fu) - 12;
        int dy = int(get_rand_32() & 0x1Fu) - 12;
        int px = sx + 12 + dx;
        int py = 32 + dy;
        if (px >= 0 && px < SSD1306::W && py >= 0 && py < SSD1306::H) {
            _oled->setPixel(px, py, true);
        }
    }
}
