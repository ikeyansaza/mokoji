#include "display.h"
#include "sprites.h"
#include "kana.h"
#include "font.h"
#include "pico/rand.h"
#include <cstdio>
#include <cstring>

Display::Display(SSD1306* oled) : _oled(oled) {}

void Display::draw(const Game& g) {
    _oled->clear();

    // 就寝中でもメニューは開ける（寝顔を撫でる用）ので、MENU 画面のときは通常描画に回す。
    if (g.sleeping() && g.screen() != Game::Screen::MENU) {
        drawSleep(g);
        return;
    }
    switch (g.screen()) {
        case Game::Screen::MAIN:   drawMain(g);   break;
        case Game::Screen::MENU:   drawMenu(g);   break;
        case Game::Screen::GRAVE:  drawGrave(g);  break;
        case Game::Screen::NAMING: drawNaming(g); break;
        case Game::Screen::MINIGAME: drawMinigame(g); break;
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

    // 毛キラキラはモコ・サフォーク系のみ（ワイルド系は wool 概念がないため非表示）。
    // 旧 save / BABY 時代に溜まった wool が YOUNG_WILD に持ち越されてもキラキラを出さない。
    if (g.wool() > 30 && g.family() != Game::Family::WILD) {
        drawWool(g.wool(), sx);
    }
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
        _oled->drawKana(g.nameKana(), 96, 0);
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

// kana index 列の字数（kana::END 終端、最大 kana::MAX_NAME）。
static int kanaLen(const uint8_t* s) {
    int n = 0;
    while (n < kana::MAX_NAME && s[n] != kana::END) ++n;
    return n;
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
    _oled->drawKana(g.nameKana(), 96, 0);

    // 中段：選択中の項目だけを 2 倍（日本語 16x16）で中央に出す。余白を確保するため 1 項目のみ。
    // 左右の < > は「L/R で切り替えられる」合図、下のドットは 5 項目中の現在位置。
    int cursor = g.menuCursor();
    const char* label = g.menuLabel(cursor);
    constexpr int scale = 2;
    int width = font::textWidth(label) * scale;
    _oled->drawText(label, (SSD1306::W - width) / 2, 24, false, scale);
    _oled->drawText("<", 12, 28);
    _oled->drawText(">", SSD1306::W - 12 - font::CHAR_W, 28);

    // 下段：ページドット。現在位置は 4x4 の塗り、それ以外は 2x2。
    constexpr int dot_pitch = 10;
    for (int i = 0; i < Game::MENU_COUNT; ++i) {
        int cx = SSD1306::W / 2 + (i - Game::MENU_COUNT / 2) * dot_pitch;
        if (i == cursor) {
            _oled->fillRect(cx - 2, 52, 4, 4, true);
        } else {
            _oled->fillRect(cx - 1, 53, 2, 2, true);
        }
    }
}

// スプライトの非空白部分の最下行（rotCW のときは回転後の最下行）。スプライトは下に余白を
// 持つので、これで地面に接地させる（余白のまま置くと羊が浮いて見える）。
static int spriteBottom(const uint8_t (*sp)[3], bool rotCW) {
    int bottom = 0;
    for (int r = 0; r < 24; ++r) {
        for (int c = 0; c < 24; ++c) {
            if ((sp[r][c >> 3] >> (7 - (c & 7))) & 1) {
                int y = rotCW ? c : r;   // 時計回り 90° では、元の col が回転後の row になる
                if (y > bottom) bottom = y;
            }
        }
    }
    return bottom;
}

// ミニゲーム「柵を跳ぶ羊」。地面 y=52、羊は x=16 に 24x24 で立つ。
void Display::drawMinigame(const Game& g) {
    const JumpGame& j = g.jump();
    constexpr int GROUND_Y = 52;
    char buf[24];

    _oled->fillRect(0, GROUND_Y, SSD1306::W, 1, true);

    const bool over = (j.state() == JumpGame::State::OVER);
    auto sprite = selectSprite(g, Game::Face::RIGHT);
    if (over) {
        // 転んだ羊：スプライトを時計回りに 90° 回して地面に横たえる
        int top = GROUND_Y - 1 - spriteBottom(sprite, true);
        _oled->drawSpriteRotCW(sprite, JumpGame::SHEEP_X, top);
    } else {
        // ジャンプ中は高さぶん持ち上げる
        int top = GROUND_Y - 1 - spriteBottom(sprite, false);
        _oled->drawSprite(sprite, JumpGame::SHEEP_X, top - j.sheepHeight());
    }

    // ぶつかった柵：終了時に当たり判定の範囲にある、羊に最も近い 1 本
    int hit = -1;
    if (over) {
        int best = (JumpGame::FENCE_W + JumpGame::HITBOX_W) / 2;
        for (int i = 0; i < JumpGame::MAX_FENCES; ++i) {
            if (!j.fence(i).used) continue;
            int dx = j.fenceCenterX(i) - JumpGame::HITBOX_CENTER_X;
            if (dx < 0) dx = -dx;
            if (dx < best) { best = dx; hit = i; }
        }
    }

    // 柵：中心 x から幅 FENCE_W・高さ FENCE_H の長方形。画面外のものは描かない。
    // ぶつかった柵は倒れた（横向きの）長方形にする。
    for (int i = 0; i < JumpGame::MAX_FENCES; ++i) {
        if (!j.fence(i).used) continue;
        int cx = j.fenceCenterX(i);
        if (cx < -JumpGame::FENCE_H || cx > SSD1306::W + JumpGame::FENCE_W) continue;
        if (i == hit) {
            _oled->fillRect(cx - JumpGame::FENCE_H / 2, GROUND_Y - JumpGame::FENCE_W,
                            JumpGame::FENCE_H, JumpGame::FENCE_W, true);
        } else {
            _oled->fillRect(cx - JumpGame::FENCE_W / 2, GROUND_Y - JumpGame::FENCE_H,
                            JumpGame::FENCE_W, JumpGame::FENCE_H, true);
        }
    }

    switch (j.state()) {
        case JumpGame::State::COUNTDOWN: {
            std::snprintf(buf, sizeof(buf), "%d", j.countdownNumber());
            constexpr int scale = 3;
            int w = font::textWidth(buf) * scale;
            _oled->drawText(buf, (SSD1306::W - w) / 2, 12, false, scale);
            break;
        }
        case JumpGame::State::PLAYING: {
            std::snprintf(buf, sizeof(buf), "%d連続", j.score());
            _oled->drawText(buf, SSD1306::W - font::textWidth(buf), 0);
            break;
        }
        case JumpGame::State::OVER: {
            std::snprintf(buf, sizeof(buf), "%d連続", j.score());
            constexpr int scale = 2;
            int w = font::textWidth(buf) * scale;
            _oled->drawText(buf, (SSD1306::W - w) / 2, 0, false, scale);
            // 羊（地面に立つと y=28〜）と重ならないよう、上の 27px 以内に収める
            std::snprintf(buf, sizeof(buf), "しあわせ +%d", g.miniReward());
            w = font::textWidth(buf);
            _oled->drawText(buf, (SSD1306::W - w) / 2, 19);
            // ふらふらの星：約 0.2 秒ごとに 2 コマで入れ替わる（羊の右側）
            const int phase = int((j.nowMs() / 200) & 1u);
            _oled->drawText("*", phase ? 46 : 42, 32);
            _oled->drawText("*", phase ? 42 : 46, 42);
            break;
        }
        default:
            break;
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
    _oled->drawText("おもいで", 0, 0);
    int n = g.graveCount();
    // 新しい順に最大 3 件を表示（i=0 が最新）
    int show = (n > 3) ? 3 : n;
    for (int i = 0; i < show; ++i) {
        const GraveRecord& gr = g.grave(n - 1 - i);
        char line[40];
        // 天寿 = 寿命でまっとう（age）、旅立ち = 餓死/不幸死（status）
        const char* tag = (gr.death_cause == uint8_t(Game::DeathCause::AGE)) ? "天寿" : "旅立ち";
        std::snprintf(line, sizeof(line), "%-4s %-4s %d日 %s",
                      gr.name, gr.breed, int(gr.age_days), tag);
        _oled->drawText(line, 0, 16 + i * 12);
    }
    _oled->drawText("ボタンをおす", 0, 56);
}

const uint8_t (*Display::selectSprite(const Game& g, Game::Face face))[3] {
    using namespace sprites;

    const uint8_t (*L)[3] = BABY_L;
    const uint8_t (*F)[3] = BABY_F;
    const uint8_t (*R)[3] = BABY_R;

    bool fluffy   = g.isFluffy();
    bool longhorn = g.isLonghorn();

    switch (g.stage()) {
        case Game::Stage::BABY:
            L = BABY_L; F = BABY_F; R = BABY_R; break;
        case Game::Stage::YOUNG_MOKO:
            L = YOUNG_MOKO_L; F = YOUNG_MOKO_F; R = YOUNG_MOKO_R; break;
        case Game::Stage::YOUNG_SUFFOLK:
            L = YOUNG_SUFFOLK_L; F = YOUNG_SUFFOLK_F; R = YOUNG_SUFFOLK_R; break;
        case Game::Stage::YOUNG_WILD:
            L = YOUNG_WILD_L; F = YOUNG_WILD_F; R = YOUNG_WILD_R; break;
        case Game::Stage::ADULT:
            switch (g.breed()) {
                case Game::Breed::MERINO:
                    if (fluffy) {
                        L = ADULT_MERINO_FLUFFY_L; F = ADULT_MERINO_FLUFFY_F; R = ADULT_MERINO_FLUFFY_R;
                    } else {
                        L = ADULT_MERINO_L; F = ADULT_MERINO_F; R = ADULT_MERINO_R;
                    }
                    break;
                case Game::Breed::CORRIEDALE:
                    if (fluffy) {
                        L = ADULT_CORRIEDALE_FLUFFY_L; F = ADULT_CORRIEDALE_FLUFFY_F; R = ADULT_CORRIEDALE_FLUFFY_R;
                    } else {
                        L = ADULT_CORRIEDALE_L; F = ADULT_CORRIEDALE_F; R = ADULT_CORRIEDALE_R;
                    }
                    break;
                case Game::Breed::LINCOLN:
                    if (fluffy) {
                        L = ADULT_LINCOLN_FLUFFY_L; F = ADULT_LINCOLN_FLUFFY_F; R = ADULT_LINCOLN_FLUFFY_R;
                    } else {
                        L = ADULT_LINCOLN_L; F = ADULT_LINCOLN_F; R = ADULT_LINCOLN_R;
                    }
                    break;
                case Game::Breed::SUFFOLK:
                    if (fluffy) {
                        L = ADULT_SUFFOLK_FLUFFY_L; F = ADULT_SUFFOLK_FLUFFY_F; R = ADULT_SUFFOLK_FLUFFY_R;
                    } else {
                        L = ADULT_SUFFOLK_L; F = ADULT_SUFFOLK_F; R = ADULT_SUFFOLK_R;
                    }
                    break;
                case Game::Breed::HAMPSHIRE:
                    if (fluffy) {
                        L = ADULT_HAMPSHIRE_FLUFFY_L; F = ADULT_HAMPSHIRE_FLUFFY_F; R = ADULT_HAMPSHIRE_FLUFFY_R;
                    } else {
                        L = ADULT_HAMPSHIRE_L; F = ADULT_HAMPSHIRE_F; R = ADULT_HAMPSHIRE_R;
                    }
                    break;
                case Game::Breed::MOUFLON:
                    if (longhorn) {
                        L = ADULT_MOUFLON_LONGHORN_L; F = ADULT_MOUFLON_LONGHORN_F; R = ADULT_MOUFLON_LONGHORN_R;
                    } else {
                        L = ADULT_MOUFLON_L; F = ADULT_MOUFLON_F; R = ADULT_MOUFLON_R;
                    }
                    break;
                case Game::Breed::BIGHORN:
                    if (longhorn) {
                        L = ADULT_BIGHORN_LONGHORN_L; F = ADULT_BIGHORN_LONGHORN_F; R = ADULT_BIGHORN_LONGHORN_R;
                    } else {
                        L = ADULT_BIGHORN_L; F = ADULT_BIGHORN_F; R = ADULT_BIGHORN_R;
                    }
                    break;
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
// 名前・入力中の文字は 8x8 ひらがな（美咲ゴシック）で描く。
// 行ラベル（"ka-" 等）と操作ガイドは ASCII 5x7 のまま。
// =====================================================================
// 入力中の名前を「[もこ_]」の形で左上に描く。かなは 8x8、括弧とカーソルは 5x7 ASCII。
void Display::drawNameInput(const uint8_t* buffer) {
    _oled->drawText("[", 0, 0);
    _oled->drawKana(buffer, 6, 0);
    int x = 6 + kanaLen(buffer) * font::KANA_ADVANCE;
    _oled->drawText("_]", x, 0);
}

void Display::drawNaming(const Game& g) {
    char buf[32];
    switch (g.namingMode()) {
        case Game::NamingMode::SELECT_MODE: {
            _oled->drawText("なまえ", 0, 0);
            int cur = g.namingCursor();
            // 上：preset
            if (cur == 0) {
                _oled->fillRect(0, 18, 80, 12, true);
                _oled->drawText("おまかせ", 4, 20, true);
            } else {
                _oled->drawText("おまかせ", 4, 20, false);
            }
            // 下：type
            if (cur == 1) {
                _oled->fillRect(0, 36, 80, 12, true);
                _oled->drawText("じぶんで", 4, 38, true);
            } else {
                _oled->drawText("じぶんで", 4, 38, false);
            }
            _oled->drawText("< > えらぶ  OK 決定", 0, 56);
            break;
        }
        case Game::NamingMode::PRESET_PICK: {
            _oled->drawText("おまかせ", 0, 0);
            int idx = g.namingCursor();
            // 中央寄せで現在の preset 名を表示
            int width = kanaLen(kana::PRESETS[idx]) * font::KANA_ADVANCE;
            int x = (128 - width) / 2;
            if (x < 0) x = 0;
            _oled->drawKana(kana::PRESETS[idx], x, 28);
            std::snprintf(buf, sizeof(buf), "%d/%d", idx + 1, kana::PRESET_COUNT);
            _oled->drawText(buf, 0, 16);
            _oled->drawText("< > えらぶ  OK 決定", 0, 56);
            break;
        }
        case Game::NamingMode::INPUT_ROW: {
            // 上段：現在の入力バッファ
            drawNameInput(g.inputBuffer());
            // 中段：行を選ぶ
            int cur = g.namingCursor();
            const int total = kana::ROW_COUNT + 2;   // + けす + 決定
            const char* label;
            if (cur < kana::ROW_COUNT)              label = kana::ROWS[cur].label;
            else if (cur == kana::ROW_COUNT)        label = "けす";
            else                                    label = "決定";
            int width = font::textWidth(label);
            int x = (128 - width) / 2;
            if (x < 0) x = 0;
            _oled->fillRect(x - 2, 26, width + 4, 12, true);
            _oled->drawText(label, x, 28, true);
            std::snprintf(buf, sizeof(buf), "%d/%d", cur + 1, total);
            _oled->drawText(buf, 0, 44);
            _oled->drawText("< > えらぶ  OK 決定", 0, 56);
            break;
        }
        case Game::NamingMode::INPUT_CHAR: {
            drawNameInput(g.inputBuffer());
            const auto& row = kana::ROWS[g.inputRow()];
            int cur = g.namingCursor();
            uint8_t kanaIdx = uint8_t(row.start + cur);
            const uint8_t selected[2] = { kanaIdx, kana::END };
            int width = font::KANA_ADVANCE;
            int x = (128 - width) / 2;
            _oled->fillRect(x - 2, 26, width + 4, 12, true);
            _oled->drawKana(selected, x, 28, true);
            std::snprintf(buf, sizeof(buf), "%s %d/%d", row.label, cur + 1, row.length);
            _oled->drawText(buf, 0, 44);
            _oled->drawText("< > えらぶ  OK 決定", 0, 56);
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
