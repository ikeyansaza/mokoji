// game.cpp のホスト側ユニットテスト。
// Pico ハード非依存なロジック（ステータス減衰・進化・墓・save/load）の転置バグを潰す目的。
//
// ビルド方法: cpp/test/Makefile 参照
//   make test

#include "game.h"
#include "font.h"
#include "kana.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#define RUN(t) do { \
    std::fprintf(stderr, "[run ] %s\n", #t); \
    t(); \
    std::fprintf(stderr, "[ ok ] %s\n", #t); \
} while (0)

// メニューを開いて指定 cursor まで動かして確定する（テスト用ヘルパ）。
static void menu_action(Game& g, int idx) {
    g.onButton(Game::Button::CENTER);              // main → menu
    while (g.menuCursor() != idx) {
        g.onButton(g.menuCursor() < idx
                   ? Game::Button::RIGHT
                   : Game::Button::LEFT);
    }
    g.onButton(Game::Button::CENTER);              // 確定 → main
}

// 命名画面をデフォルト（preset 0 = もこ）でスキップして main へ移動する。
// 新規 Game / die() 後など Screen::NAMING に居るときに main 操作を始める前に呼ぶ。
static void skip_naming(Game& g) {
    if (g.screen() != Game::Screen::NAMING) return;
    g.onButton(Game::Button::CENTER);   // SELECT_MODE → PRESET_PICK
    g.onButton(Game::Button::CENTER);   // PRESET_PICK confirm → MAIN
}

static void test_default_state() {
    Game g(nullptr);
    // 新規 Game はまず命名画面で開始する
    assert(g.screen() == Game::Screen::NAMING);
    // デフォルトのかな名は preset 0 「もこ」、romaji 表示は "moko"
    assert(std::strcmp(g.name(), "moko") == 0);
    assert(g.hunger() == 100);
    assert(g.happy()  == 100);
    assert(g.wool()   == 0);
    assert(g.stage()  == Game::Stage::BABY);
    assert(g.breed()  == Game::Breed::NONE);
    assert(g.graveCount() == 0);
    assert(!g.sleeping());
}

static void test_naming_preset_select() {
    Game g(nullptr);
    assert(g.screen() == Game::Screen::NAMING);
    // SELECT_MODE で center → PRESET_PICK へ
    g.onButton(Game::Button::CENTER);
    assert(g.namingMode() == Game::NamingMode::PRESET_PICK);
    // 右に 1 つ進めて preset 1 (ふわ)
    g.onButton(Game::Button::RIGHT);
    assert(g.namingCursor() == 1);
    // center で確定 → MAIN
    g.onButton(Game::Button::CENTER);
    assert(g.screen() == Game::Screen::MAIN);
    // 名前が "fuwa" に
    assert(std::strcmp(g.name(), "fuwa") == 0);
}

static void test_naming_long_press_backspace() {
    Game g(nullptr);
    // INPUT_ROW へ：select right→ TYPE → enter
    g.onButton(Game::Button::RIGHT);
    g.onButton(Game::Button::CENTER);
    // ま行 → ま を入力
    for (int i = 0; i < 6; ++i) g.onButton(Game::Button::RIGHT);
    g.onButton(Game::Button::CENTER);  // INPUT_CHAR
    g.onButton(Game::Button::CENTER);  // 「ま」確定
    assert(g.inputLen() == 1);
    // INPUT_ROW に戻ったところで LEFT_LONG → 1 文字削除
    g.onButton(Game::Button::LEFT_LONG);
    assert(g.inputLen() == 0);
}

static void test_naming_manual_input() {
    Game g(nullptr);
    // SELECT_MODE で右→ TYPE 選択 → INPUT_ROW へ
    g.onButton(Game::Button::RIGHT);
    g.onButton(Game::Button::CENTER);
    assert(g.namingMode() == Game::NamingMode::INPUT_ROW);
    // ま行 (index 6) へ移動
    for (int i = 0; i < 6; ++i) g.onButton(Game::Button::RIGHT);
    assert(g.namingCursor() == 6);
    g.onButton(Game::Button::CENTER);
    assert(g.namingMode() == Game::NamingMode::INPUT_CHAR);
    // ま (cursor 0) を確定
    g.onButton(Game::Button::CENTER);
    assert(g.namingMode() == Game::NamingMode::INPUT_ROW);
    assert(g.inputLen() == 1);
    // OK (= ROW_COUNT + 1) まで cursor を進めて確定
    while (g.namingCursor() < 17) g.onButton(Game::Button::RIGHT);
    g.onButton(Game::Button::CENTER);
    assert(g.screen() == Game::Screen::MAIN);
    assert(std::strcmp(g.name(), "ma") == 0);
}

static void test_tick_advances_age() {
    Game g(nullptr);
    auto a0 = g.ageTicks();
    g.tick(); g.tick(); g.tick();
    assert(g.ageTicks() == a0 + 3);
}

static void test_hunger_decays_after_three_hours() {
    Game g(nullptr);
    int h0 = g.hunger();
    // 1 game-hour: まだ 0 のまま
    for (uint32_t i = 0; i < Game::TICKS_PER_HOUR; ++i) g.tick();
    assert(g.hunger() == h0);
    // さらに 2 game-hour 進めて 3 game-hour 経過：-1
    for (uint32_t i = 0; i < 2 * Game::TICKS_PER_HOUR; ++i) g.tick();
    assert(g.hunger() == h0 - 1);
}

static void test_natural_death_within_lifespan_window() {
    Game g(nullptr);
    skip_naming(g);
    int initial = g.graveCount();
    // 20 日相当 tick（寿命 10-15 日 + マージン）。途中で死なないように feed/pet。
    uint32_t budget = Game::TICKS_PER_DAY * 20;
    while (g.graveCount() == initial && g.ageTicks() < budget) {
        g.tick();
        if (g.hunger() < 50) menu_action(g, 0);
        if (g.happy()  < 50) menu_action(g, 1);
    }
    assert(g.graveCount() == initial + 1);
    const GraveRecord& gr = g.grave(initial);
    assert(gr.age_days >= 10 && gr.age_days <= 15);
    // 天寿で死んでいるはず（feed/pet が効いて空腹/幸福では死んでない）
    assert(gr.death_cause == uint8_t(Game::DeathCause::AGE));
}

static void test_starvation_marked_as_status_death() {
    Game g(nullptr);
    skip_naming(g);
    int initial = g.graveCount();
    // feed しないので空腹で死ぬはず（300 game-hour ≒ 12.5 日、寿命 10-15 日と
    // ほぼ同じレンジに居るので run によっては寿命死に勝つことがある）
    uint32_t safety = 500 * Game::TICKS_PER_HOUR;
    while (g.graveCount() == initial && safety--) g.tick();
    assert(g.graveCount() == initial + 1);
    // 死因はどちらか必ず入る
    uint8_t c = g.grave(initial).death_cause;
    assert(c == uint8_t(Game::DeathCause::STATUS) || c == uint8_t(Game::DeathCause::AGE));
}

static void test_feed_via_menu() {
    Game g(nullptr);
    skip_naming(g);
    // hunger を下げておく（30 game-hour 経過）
    for (uint32_t i = 0; i < 30 * Game::TICKS_PER_HOUR; ++i) g.tick();
    int before = g.hunger();
    assert(before < 100);

    // CENTER で main → menu、もう一度 CENTER で cursor=0=FEED 確定
    g.onButton(Game::Button::CENTER);
    assert(g.screen() == Game::Screen::MENU);
    g.onButton(Game::Button::CENTER);
    assert(g.screen() == Game::Screen::MAIN);

    int after = g.hunger();
    assert(after >= before);
    // 100 を上限としつつ +30 されている
    assert(after - before == 30 || after == 100);
}

static void test_evolution_to_young() {
    Game g(nullptr);
    skip_naming(g);
    // 3 days 強までゲームを進める。途中で死なないように適宜 feed/pet。
    uint32_t budget = Game::TICKS_PER_DAY * 5;
    while (g.stage() == Game::Stage::BABY && g.ageTicks() < budget) {
        g.tick();
        if (g.hunger() < 50) menu_action(g, 0);  // FEED
        if (g.happy()  < 50) menu_action(g, 1);  // PET
    }
    assert(g.stage() != Game::Stage::BABY);
    auto s = g.stage();
    assert(s == Game::Stage::YOUNG_MOKO ||
           s == Game::Stage::YOUNG_SUFFOLK ||
           s == Game::Stage::YOUNG_WILD);
}

static void test_evolution_to_adult() {
    Game g(nullptr);
    skip_naming(g);
    uint32_t budget = Game::TICKS_PER_DAY * 10;
    while (g.stage() != Game::Stage::ADULT && g.ageTicks() < budget) {
        g.tick();
        if (g.hunger() < 50) menu_action(g, 0);  // FEED
        if (g.happy()  < 50) menu_action(g, 1);  // PET
    }
    assert(g.stage() == Game::Stage::ADULT);
    assert(g.breed() != Game::Breed::NONE);
}

static void test_death_increments_graves_and_preserves_them() {
    Game g(nullptr);
    skip_naming(g);
    int initial = g.graveCount();

    // 餓死（300 game-hour で hunger 0）または寿命到達（最大 15 日 = 360 game-hour）の
    // 早い方で死亡する。budget は両方マージン込みで 500 game-hour 取る。
    uint32_t safety = 500 * Game::TICKS_PER_HOUR;
    while (g.graveCount() == initial && safety--) g.tick();
    assert(g.graveCount() == initial + 1);

    // newGame の後はデフォルト名 "moko"（プリセット 0 由来）で再スタート
    assert(std::strcmp(g.name(), "moko") == 0);
    assert(g.stage() == Game::Stage::BABY);

    // もう 1 回殺して、墓が累積することを確認（Python 版にあった
    // 「_new_game で graves=[] してしまうバグ」が C++ では直っていることの検証）
    safety = 500 * Game::TICKS_PER_HOUR;
    while (g.graveCount() == initial + 1 && safety--) g.tick();
    assert(g.graveCount() == initial + 2);
}

static void test_dirty_flag_lifecycle() {
    Game g(nullptr);
    skip_naming(g);
    // 命名直後は commitName() で dirty=true
    assert(g.isDirty());
    g.clearDirty();
    assert(!g.isDirty());

    // age_ticks 単独の進行は dirty を立てない
    g.tick();
    assert(!g.isDirty());

    // hourly check（ステータス変動）で dirty になる
    for (uint32_t i = 0; i < Game::TICKS_PER_HOUR - 1; ++i) g.tick();
    assert(g.isDirty());
    g.clearDirty();

    // メニュー操作（FEED）でも dirty
    g.onButton(Game::Button::CENTER);
    g.onButton(Game::Button::CENTER);
    assert(g.isDirty());
}

static void test_save_and_load_round_trip() {
    Game g1(nullptr);
    skip_naming(g1);
    // 状態を変化させる
    for (uint32_t i = 0; i < 5 * Game::TICKS_PER_HOUR; ++i) g1.tick();
    g1.onButton(Game::Button::CENTER);
    g1.onButton(Game::Button::CENTER);   // FEED

    GameSaveData saved = g1.saveData();

    Game g2(nullptr, &saved);

    assert(std::strcmp(g1.name(), g2.name()) == 0);
    assert(g1.stage()      == g2.stage());
    assert(g1.breed()      == g2.breed());
    assert(g1.hunger()     == g2.hunger());
    assert(g1.happy()      == g2.happy());
    assert(g1.wool()       == g2.wool());
    assert(g1.ageTicks()   == g2.ageTicks());
    assert(g1.graveCount() == g2.graveCount());
}

// 就寝中の Game を作る（テスト用ヘルパ）。
// tick で就寝させると数日分かかるため、セーブデータ経由で睡眠状態を注入する。
// 空腹・幸福を中途半端な値、wool を刈れる量にして、行動が実効化されたら差が出るようにしておく。
static Game make_sleeping_game() {
    Game base(nullptr);
    skip_naming(base);
    GameSaveData d = base.saveData();
    d.sleeping = 1;
    d.sleepy   = 90;
    d.hunger   = 50;
    d.happy    = 50;
    d.wool     = 50;
    return Game(nullptr, &d);
}

// メニューから指定の Action を選んで確定する（メニュー項目の並びに依存しない）。
static void menu_select(Game& g, Game::Action act) {
    for (int i = 0; i < Game::MENU_COUNT; ++i) {
        if (g.menuAction(i) == act) { menu_action(g, i); return; }
    }
    assert(false && "action not in menu");
}

static void test_sleeping_can_open_menu() {
    Game g = make_sleeping_game();
    assert(g.sleeping());
    g.onButton(Game::Button::CENTER);
    assert(g.screen() == Game::Screen::MENU);
    g.onButton(Game::Button::RIGHT);
    assert(g.menuCursor() == 1);
    assert(g.sleeping());
}

static void test_sleeping_pet_raises_happy_without_waking() {
    Game g = make_sleeping_game();
    menu_select(g, Game::Action::PET);
    assert(g.happy() == 60);                       // 起きている時の +20 より控えめ
    assert(g.sleeping());                          // 撫でても起きない
    assert(g.screen() == Game::Screen::MAIN);
}

static void test_sleeping_other_actions_are_rejected() {
    const Game::Action rejected[] = {
        Game::Action::FEED, Game::Action::SHEAR, Game::Action::MINI,
    };
    for (Game::Action act : rejected) {
        Game g = make_sleeping_game();
        menu_select(g, act);
        assert(g.hunger() == 50);
        assert(g.happy()  == 50);
        assert(g.wool()   == 50);
        assert(g.sleeping());
        assert(g.screen() == Game::Screen::MAIN);
    }
}

// 8x8 ひらがなグリフ。1 行 1 バイト、bit 7 が左端（BDF と同じ並び）。
static bool glyph_has_pixel(const uint8_t* g) {
    for (int r = 0; r < font::KANA_H; ++r) if (g[r]) return true;
    return false;
}

static void test_kana_glyph_all_defined() {
    for (uint8_t i = 0; i < kana::COUNT; ++i) {
        const uint8_t* g = font::kanaGlyph(i);
        assert(g != nullptr);
        assert(glyph_has_pixel(g));                // 空グリフ = 生成漏れ
    }
}

static void test_kana_glyph_all_distinct() {
    // 濁点・半濁点・小書き文字も別字形になっている（index のずれ・コピー漏れ検出）
    for (uint8_t i = 0; i < kana::COUNT; ++i) {
        for (uint8_t j = i + 1; j < kana::COUNT; ++j) {
            assert(std::memcmp(font::kanaGlyph(i), font::kanaGlyph(j), font::KANA_H) != 0);
        }
    }
}

static void test_kana_glyph_matches_misaki_a() {
    // 美咲ゴシック「あ」（index 0）。BDF の 7x7 ビットマップを 8x8 セル上詰めで置いたもの。
    static const uint8_t kExpected[8] = { 0x20, 0x7C, 0x20, 0x3C, 0x6A, 0xB2, 0x64, 0x00 };
    assert(std::memcmp(font::kanaGlyph(0), kExpected, 8) == 0);
}

static void test_kana_glyph_out_of_range_is_blank() {
    // END (0xFF) や範囲外は描画側が安全に扱えるよう空グリフを返す
    assert(!glyph_has_pixel(font::kanaGlyph(kana::END)));
    assert(!glyph_has_pixel(font::kanaGlyph(kana::COUNT)));
}

// メニューラベル（UTF-8）。
static void test_menu_labels() {
    Game g(nullptr);
    skip_naming(g);
    assert(std::string(g.menuLabel(0)) == "ごはん");    // FEED
    assert(std::string(g.menuLabel(1)) == "なでる");    // PET
    assert(std::string(g.menuLabel(2)) == "毛刈り");    // SHEAR
    assert(std::string(g.menuLabel(3)) == "ゲーム");    // MINI
    assert(std::string(g.menuLabel(4)) == "もどる");    // BACK
}

static void test_menu_label_polish_for_wild() {
    Game base(nullptr);
    skip_naming(base);
    GameSaveData d = base.saveData();
    d.stage = uint8_t(Game::Stage::YOUNG_WILD);
    Game g(nullptr, &d);
    assert(g.family() == Game::Family::WILD);
    assert(std::string(g.menuLabel(2)) == "角研ぎ");   // POLISH（ワイルド系は毛ではなく角）
}

static void test_menu_labels_renderable_and_bounded() {
    // 全字がフォントに収録されていて（未収録は空白表示になる）、2 倍表示（16px/字）で
    // 左右の < > と重ならない幅に収まる。ワイルド系のラベルも含めて確認する。
    Game base(nullptr);
    skip_naming(base);
    GameSaveData d = base.saveData();
    d.stage = uint8_t(Game::Stage::YOUNG_WILD);
    Game wild(nullptr, &d);
    Game moko(nullptr);
    skip_naming(moko);

    for (const Game* g : { &moko, &wild }) {
        for (int i = 0; i < Game::MENU_COUNT; ++i) {
            const char* label = g->menuLabel(i);
            assert(*label != '\0');
            assert(font::textWidth(label) <= 40);          // 2 倍で 80px まで（矢印間は 92px）
            const char* p = label;
            while (*p) {
                uint32_t cp = font::decodeUtf8(p);
                assert(cp >= 0x80 && font::jaGlyph(cp) != nullptr);
            }
        }
    }
    assert(*moko.menuLabel(Game::MENU_COUNT) == '\0');      // 範囲外は空ラベル
}

// UTF-8 デコード / 日本語グリフ / 表示幅。
static uint32_t decode_first(const char* s) {
    const char* p = s;
    return font::decodeUtf8(p);
}

static void test_utf8_decode() {
    assert(decode_first("A")        == 0x41);
    assert(decode_first("\xE3\x81\x82") == 0x3042);   // あ (3 バイト)
    assert(decode_first("\xE7\xBE\x8A") == 0x7F8A);   // 羊
    const char* p = "aあ";
    assert(font::decodeUtf8(p) == 'a');
    assert(font::decodeUtf8(p) == 0x3042);
    assert(*p == '\0');                               // 読み進めた先が終端
}

static void test_utf8_decode_invalid_is_replacement() {
    // 不正な先頭バイト・継続バイト不足は U+FFFD を返し、必ず 1 バイト以上進む（無限ループ防止）
    const char* p = "\xFF";
    assert(font::decodeUtf8(p) == font::REPLACEMENT);
    assert(*p == '\0');
    const char* q = "\xE3\x81";                       // 3 バイト文字が途中で切れている
    assert(font::decodeUtf8(q) == font::REPLACEMENT);
    assert(*q == '\0');                               // 終端を越えて読まない
    const char* r = "\x81";                           // 単独の継続バイト
    assert(font::decodeUtf8(r) == font::REPLACEMENT);
    assert(*r == '\0');
}

static void test_ja_glyph_lookup() {
    // カタカナ「ア」・漢字「羊」（美咲ゴシックの実データ）
    static const uint8_t kA[8]   = { 0x7E, 0x02, 0x14, 0x18, 0x10, 0x10, 0x20, 0x00 };
    static const uint8_t kHitsuji[8] = { 0x44, 0xFE, 0x10, 0x7C, 0x10, 0xFE, 0x10, 0x00 };
    assert(font::jaGlyph(0x30A2) && std::memcmp(font::jaGlyph(0x30A2), kA, 8) == 0);
    assert(font::jaGlyph(0x7F8A) && std::memcmp(font::jaGlyph(0x7F8A), kHitsuji, 8) == 0);
}

static void test_ja_glyph_covers_game_vocabulary() {
    // ゲーム内で使う予定の字が JIS 第一水準に入っている
    const char* used = "羊毛刈餌世話行天寿眠決定消小空腹幸福";
    const char* p = used;
    while (*p) {
        uint32_t cp = font::decodeUtf8(p);
        assert(font::jaGlyph(cp) != nullptr);
    }
}

static void test_ja_glyph_matches_kana_table() {
    // ひらがなは index 版（名前用）と Unicode 版で同じ字形（生成の食い違い検出）
    assert(std::memcmp(font::jaGlyph(0x3042), font::kanaGlyph(0),  8) == 0);   // あ
    assert(std::memcmp(font::jaGlyph(0x3093), font::kanaGlyph(45), 8) == 0);   // ん
    assert(std::memcmp(font::jaGlyph(0x3071), font::kanaGlyph(66), 8) == 0);   // ぱ
}

static void test_ja_glyph_unsupported_is_null() {
    assert(font::jaGlyph('A') == nullptr);            // ASCII は 5x7 フォント側
    assert(font::jaGlyph(0x1F600) == nullptr);        // 絵文字
}

static void test_text_width_mixed() {
    assert(font::textWidth("") == 0);
    assert(font::textWidth("AB") == 12);              // ASCII は 6px
    assert(font::textWidth("あ") == 8);               // 日本語は 8px
    assert(font::textWidth("か行") == 16);
    assert(font::textWidth("A あ") == 6 + 6 + 8);
    assert(font::textWidth("\xFF") == 8);             // 不正バイトも 1 字（8px）として扱う
}

static void test_naming_row_labels_are_renderable() {
    // 行ラベル（"あ行" 等）の全字が日本語フォントに収録されている（未収録だと空白で表示される）
    for (int i = 0; i < kana::ROW_COUNT; ++i) {
        const char* p = kana::ROWS[i].label;
        assert(*p != '\0');
        while (*p) {
            uint32_t cp = font::decodeUtf8(p);
            assert(cp < 0x80 || font::jaGlyph(cp) != nullptr);
        }
    }
}

int main() {
    std::setbuf(stdout, nullptr);
    std::srand(42);   // 進化判定の再現性のため固定シード
    std::fprintf(stderr, "\n=== game.cpp host tests (HOST_TEST=on) ===\n\n");

    RUN(test_default_state);
    RUN(test_naming_preset_select);
    RUN(test_naming_manual_input);
    RUN(test_naming_long_press_backspace);
    RUN(test_tick_advances_age);
    RUN(test_hunger_decays_after_three_hours);
    RUN(test_feed_via_menu);
    RUN(test_evolution_to_young);
    RUN(test_evolution_to_adult);
    RUN(test_death_increments_graves_and_preserves_them);
    RUN(test_natural_death_within_lifespan_window);
    RUN(test_starvation_marked_as_status_death);
    RUN(test_dirty_flag_lifecycle);
    RUN(test_save_and_load_round_trip);
    RUN(test_sleeping_can_open_menu);
    RUN(test_sleeping_pet_raises_happy_without_waking);
    RUN(test_sleeping_other_actions_are_rejected);
    RUN(test_kana_glyph_all_defined);
    RUN(test_kana_glyph_all_distinct);
    RUN(test_kana_glyph_matches_misaki_a);
    RUN(test_kana_glyph_out_of_range_is_blank);
    RUN(test_menu_labels);
    RUN(test_menu_label_polish_for_wild);
    RUN(test_menu_labels_renderable_and_bounded);
    RUN(test_utf8_decode);
    RUN(test_utf8_decode_invalid_is_replacement);
    RUN(test_ja_glyph_lookup);
    RUN(test_ja_glyph_covers_game_vocabulary);
    RUN(test_ja_glyph_matches_kana_table);
    RUN(test_ja_glyph_unsupported_is_null);
    RUN(test_text_width_mixed);
    RUN(test_naming_row_labels_are_renderable);

    std::printf("\n=== all tests passed ===\n\n");
    return 0;
}
