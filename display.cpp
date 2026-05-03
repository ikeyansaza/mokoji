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
    // ふわふわアイドル：12 tick 周期で 0/-1/-2/-1 と上下バウンス
    static constexpr int8_t kBob[4] = { 0, -1, -2, -1 };
    int bob = kBob[(g.ageTicks() / 6) & 3];

    int sx = g.walkX();
    int sy = 12 + bob;

    // 羊本体（2 倍スケール = 48x48）
    auto sprite = selectSprite(g, g.face());
    _oled->drawSprite2x(sprite, sx, sy);

    if (g.wool() > 30) drawWool(g.wool(), sx);
    drawActionFx(g);

    // 表情オーバーレイは SVG 参考デザインに合わせて当面オフ（スプライト自体に
    // 目・鼻が描き込まれているので overlay すると二重になる）。
    // 必要なら drawFaceFx の座標を新スプライト基準に書き直して有効化する。
    // if (g.face() == Game::Face::FRONT) drawFaceFx(g, sx, sy);
    (void)0;

    // ステータス overlay：LEFT ボタン押下中だけ表示
    if (g.leftHeld()) {
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
    }
}

void Display::drawFaceFx(const Game& g, int sx, int sy) {
    // チビ系スプライトの目位置（24x24 ソース）：
    //   左目: cols 8-9, rows 6-7（ネガティブ穴）
    //   右目: cols 14-15, rows 6-7
    // 2x スケール後：
    //   左目: cols 16-19, rows 12-15 (4x4 area)
    //   右目: cols 28-31, rows 12-15
    constexpr int LEYE_X = 16, REYE_X = 28, EYE_Y = 12;

    // === まばたき：~5 秒に 1 回、4 tick だけ目を閉じる ===
    bool blinking = (g.ageTicks() % 100) < 4;
    if (blinking) {
        // 目の中央に水平線（薄目）
        _oled->fillRect(sx + LEYE_X, sy + EYE_Y + 1, 4, 2, true);
        _oled->fillRect(sx + REYE_X, sy + EYE_Y + 1, 4, 2, true);
    } else {
        // === 通常時：目のハイライト（光る点を入れる）===
        // 黒目の中の左上に 1 pixel 灯すと「目に光が宿る」感
        _oled->setPixel(sx + LEYE_X + 1, sy + EYE_Y + 1, true);
        _oled->setPixel(sx + REYE_X + 1, sy + EYE_Y + 1, true);
    }

    // === 状態別の口表情（base sprite の口の上に追加描画）===
    // base 口位置: cols 10-13, row 9（ネガティブ穴）→ 2x: cols 20-27, rows 18-19
    if (g.hunger() < 30 || g.happy() < 30) {
        // ピンチ：口元に「><」っぽい短いライン追加（困った顔）
        _oled->setPixel(sx + 22, sy + 21, true);
        _oled->setPixel(sx + 25, sy + 21, true);
    } else if (g.hunger() > 80 && g.happy() > 80) {
        // 大満足：口角を上げる小さい点を追加（ニッコリ強調）
        _oled->setPixel(sx + 19, sy + 18, true);
        _oled->setPixel(sx + 28, sy + 18, true);
    }
}

void Display::drawActionFx(const Game& g) {
    if (g.action() == Game::Action::NONE) return;
    int sx = g.walkX();
    int t  = g.walkTick();   // 0..40

    // 2x スケール（48x48）の羊基準。羊は (sx, 12) 〜 (sx+48, 60) を占める。
    switch (g.action()) {
        case Game::Action::FEED: {
            // 羊の口元（中央下寄り）に「もぐもぐ」点滅エフェクト。
            const char* frames[] = { ".", "o", "O", "o" };
            const char* mark = frames[(t / 5) & 3];
            _oled->drawText(mark, sx + 44, 36);
            break;
        }
        case Game::Action::PET: {
            // 羊の頭の上にハート「<3」がふわっと上に浮く。
            int dy = -(t / 4);
            int y  = 4 + dy;
            if (y >= 0) _oled->drawText("<3", sx + 16, y);
            break;
        }
        case Game::Action::SHEAR: {
            // 羊の上で「><」（ハサミ）がパチパチ動く。
            const char* mark = ((t / 4) & 1) ? "><" : "X.";
            _oled->drawText(mark, sx + 16, 4);
            break;
        }
        case Game::Action::MINI: {
            _oled->drawText("*", sx + 20, 4);
            break;
        }
        default: break;
    }
}

void Display::drawMenu(const Game& g) {
    // 上段：ステータス（メニュー操作中なので常時表示で意思決定の材料に）
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

    // 下段：メニュー項目（5 項目、画面幅 128 px に収める）
    int cursor = g.menuCursor();
    constexpr int item_w = 25;
    for (int i = 0; i < Game::MENU_COUNT; ++i) {
        int x = i * item_w;
        if (i == cursor) {
            _oled->fillRect(x, 52, item_w - 1, 12, true);
            _oled->drawText(Game::menuLabel(i), x + 1, 54, true);
        } else {
            _oled->drawText(Game::menuLabel(i), x + 1, 54, false);
        }
    }
}

void Display::drawSleep(const Game& g) {
    // 焼き付き対策：スプライトと "zzz..." を ~8 秒周期で ±2px ドリフトさせる。
    static constexpr int8_t kDrift[8] = { 0, 1, 2, 1, 0, -1, -2, -1 };
    int dx = kDrift[(g.ageTicks() / 20) & 7];

    // 起きてる時と同じ 2x スケールで表示（サイズが急に変わって「消えた→現れた」と
    // 見えるのを防ぐ）。羊の中央を画面中央寄りに配置。
    auto sprite = selectSprite(g, Game::Face::FRONT);
    _oled->drawSprite2x(sprite, 40 + dx, 12);

    // "zzz..." は左上に小さく
    _oled->drawText("zzz", 0, 0);
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
    // 2x スケール羊（48x48 @ y=12）の中心は (sx+24, 36)。
    // その周辺にキラキラ散らす（範囲も 2 倍に）。
    for (int i = 0; i < dots; ++i) {
        int dx = int(get_rand_32() & 0x3Fu) - 24;
        int dy = int(get_rand_32() & 0x3Fu) - 24;
        int px = sx + 24 + dx;
        int py = 36 + dy;
        if (px >= 0 && px < SSD1306::W && py >= 0 && py < SSD1306::H) {
            _oled->setPixel(px, py, true);
        }
    }
}
