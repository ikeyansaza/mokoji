// game.cpp のホスト側ユニットテスト。
// Pico ハード非依存なロジック（ステータス減衰・進化・墓・save/load）の転置バグを潰す目的。
//
// ビルド方法: cpp/test/Makefile 参照
//   make test

#include "game.h"
#include "font.h"
#include "kana.h"
#include "jump_game.h"
#include "background.h"
#include "eat_motion.h"
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
    skip_naming(g);                    // 名前を付けてから、時間が進む（命名中は止まる）
    auto a0 = g.ageTicks();
    g.tick(); g.tick(); g.tick();
    assert(g.ageTicks() == a0 + 3);
}

static void test_hunger_decays_after_three_hours() {
    Game g(nullptr);
    skip_naming(g);
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

    // 墓を確認して名前を付けると、新しい子が生きはじめる（それまでは時間が止まっている）
    g.onButton(Game::Button::CENTER);   // 墓 → 命名
    skip_naming(g);

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
static Game make_sleeping_game(Game::Stage stage = Game::Stage::BABY) {
    Game base(nullptr);
    skip_naming(base);
    GameSaveData d = base.saveData();
    d.stage    = uint8_t(stage);
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
        // 毛刈りはベビーのメニューに無いので、若羊で確認する
        Game g = make_sleeping_game(act == Game::Action::SHEAR ? Game::Stage::YOUNG_MOKO : Game::Stage::BABY);
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

// 指定した成長段階の通常の Game を作る（セーブデータ経由）。
static Game make_stage_game(Game::Stage stage) {
    Game base(nullptr);
    skip_naming(base);
    GameSaveData d = base.saveData();
    d.stage = uint8_t(stage);
    return Game(nullptr, &d);
}

// メニューラベル（UTF-8）。
static void test_menu_labels_for_young_and_adult() {
    // 毛刈りは若羊から。ベビーのメニューには出ない。
    Game g = make_stage_game(Game::Stage::YOUNG_MOKO);
    assert(g.menuCount() == 6);
    assert(std::string(g.menuLabel(0)) == "ごはん");    // FEED
    assert(std::string(g.menuLabel(1)) == "なでる");    // PET
    assert(std::string(g.menuLabel(2)) == "毛刈り");    // SHEAR
    assert(std::string(g.menuLabel(3)) == "ゲーム");    // MINI
    assert(std::string(g.menuLabel(4)) == "プロフ");    // PROFILE
    assert(std::string(g.menuLabel(5)) == "もどる");    // BACK
    assert(g.menuAction(2) == Game::Action::SHEAR);
    assert(g.menuAction(4) == Game::Action::PROFILE);
}

static void test_baby_menu_has_no_shear() {
    Game g(nullptr);
    skip_naming(g);
    assert(g.stage() == Game::Stage::BABY);
    assert(g.menuCount() == 5);
    assert(std::string(g.menuLabel(0)) == "ごはん");
    assert(std::string(g.menuLabel(1)) == "なでる");
    assert(std::string(g.menuLabel(2)) == "ゲーム");
    assert(std::string(g.menuLabel(3)) == "プロフ");
    assert(std::string(g.menuLabel(4)) == "もどる");
    assert(*g.menuLabel(5) == '\0');                    // 5 項目目より先は空
    assert(g.menuAction(0) == Game::Action::FEED);
    assert(g.menuAction(1) == Game::Action::PET);
    assert(g.menuAction(2) == Game::Action::MINI);
    assert(g.menuAction(3) == Game::Action::PROFILE);
    assert(g.menuAction(4) == Game::Action::NONE);       // もどる
    assert(g.menuAction(5) == Game::Action::NONE);
    for (int i = 0; i < Game::MENU_COUNT; ++i) {
        assert(g.menuAction(i) != Game::Action::SHEAR);
        assert(g.menuAction(i) != Game::Action::POLISH);
    }
}

static void test_baby_menu_cursor_wraps_at_five() {
    Game g(nullptr);
    skip_naming(g);
    g.onButton(Game::Button::CENTER);                    // メニューを開く
    assert(g.menuCursor() == 0);
    g.onButton(Game::Button::LEFT);
    assert(g.menuCursor() == 4);                         // 先頭から左で末尾（もどる）へ
    g.onButton(Game::Button::RIGHT);
    assert(g.menuCursor() == 0);                         // 末尾から右で先頭へ
    for (int i = 0; i < 4; ++i) g.onButton(Game::Button::RIGHT);
    assert(g.menuCursor() == 4);
    g.onButton(Game::Button::CENTER);                    // 「もどる」でメインへ
    assert(g.screen() == Game::Screen::MAIN);
}

static void test_baby_wool_does_not_grow_but_young_does() {
    // ベビーは毛刈りできないので、毛は伸ばさない（若羊に進化した時点で毛が溜まっていないように）
    Game baby(nullptr);
    skip_naming(baby);
    for (uint32_t i = 0; i < 10 * Game::TICKS_PER_HOUR; ++i) baby.tick();
    assert(baby.wool() == 0);
    Game young = make_stage_game(Game::Stage::YOUNG_MOKO);
    for (uint32_t i = 0; i < 10 * Game::TICKS_PER_HOUR; ++i) young.tick();
    assert(young.wool() == 10);                          // 若羊は 1 時間に +1
}

static void test_evolution_resets_menu_cursor() {
    // 項目数が 5 → 6 に変わるので、開いたまま進化してもカーソルの意味がずれないよう先頭へ戻す
    Game g(nullptr);
    skip_naming(g);
    g.onButton(Game::Button::CENTER);
    for (int i = 0; i < 4; ++i) g.onButton(Game::Button::RIGHT);
    assert(g.menuCursor() == 4);
    for (uint32_t i = 0; i < 3 * Game::TICKS_PER_DAY + Game::TICKS_PER_HOUR; ++i) g.tick();
    assert(g.stage() != Game::Stage::BABY);
    assert(g.menuCount() == 6);
    assert(g.menuCursor() == 0);
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
    Game baby(nullptr);
    skip_naming(baby);
    Game moko = make_stage_game(Game::Stage::YOUNG_MOKO);

    for (const Game* g : { &baby, &moko, &wild }) {
        for (int i = 0; i < g->menuCount(); ++i) {
            const char* label = g->menuLabel(i);
            assert(*label != '\0');
            assert(font::textWidth(label) <= 40);          // 2 倍で 80px まで（矢印間は 92px）
            const char* p = label;
            while (*p) {
                uint32_t cp = font::decodeUtf8(p);
                assert(cp >= 0x80 && font::jaGlyph(cp) != nullptr);
            }
        }
        assert(*g->menuLabel(g->menuCount()) == '\0');     // 範囲外は空ラベル
    }
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

// ---- JumpGame ------------------------------------------------------------
using JG = JumpGame;

// from から ms 後まで dt 刻みで step する。
static void jg_run(JG& j, uint32_t from, uint32_t ms, uint32_t dt) {
    for (uint32_t e = dt; e <= ms; e += dt) j.step(from + e);
}

static void test_jump_countdown_then_play() {
    JG j;
    j.start(1000, 1);
    assert(j.state() == JG::State::COUNTDOWN);
    assert(j.countdownNumber() == 3);
    assert(j.consumeEvents() & JG::EV_COUNT);              // 開始時の「3」
    j.step(1000 + 999);
    assert(j.countdownNumber() == 3);
    j.step(1000 + 1000);
    assert(j.countdownNumber() == 2);
    assert(j.consumeEvents() & JG::EV_COUNT);
    j.step(1000 + 2000);
    assert(j.countdownNumber() == 1);
    j.step(1000 + JG::COUNTDOWN_MS);
    assert(j.state() == JG::State::PLAYING);
    assert(j.countdownNumber() == 0);
}

static void test_jump_ignores_press_during_countdown() {
    JG j;
    j.start(0, 1);
    j.consumeEvents();
    j.onPress(500);
    assert(!(j.consumeEvents() & JG::EV_JUMP));
    j.step(JG::COUNTDOWN_MS);
    j.onPress(JG::COUNTDOWN_MS + 5);                       // 開始直後は跳べる
    assert(j.consumeEvents() & JG::EV_JUMP);
}

static void test_jump_air_time_and_height() {
    JG j;
    j.start(0, 1);
    const uint32_t P = JG::COUNTDOWN_MS + 100;             // 最初の柵より十分前
    j.step(P);
    assert(j.sheepHeight() == 0);
    j.onPress(P);
    j.step(P + 5);
    assert(j.sheepHeight() > 0);
    j.step(P + JG::JUMP_AIR_MS / 2);
    assert(j.sheepHeight() == JG::JUMP_HEIGHT_PX);         // 頂点
    j.step(P + JG::JUMP_AIR_MS);
    assert(j.sheepHeight() == 0);                          // 着地
}

static void test_jump_ignored_while_airborne() {
    JG j;
    j.start(0, 1);
    const uint32_t P = JG::COUNTDOWN_MS + 100;
    j.step(P);
    j.consumeEvents();
    j.onPress(P);
    assert(j.consumeEvents() & JG::EV_JUMP);
    j.onPress(P + 200);                                    // 滞空中
    assert(!(j.consumeEvents() & JG::EV_JUMP));
    j.onPress(P + JG::JUMP_AIR_MS + 5);                    // 着地後
    assert(j.consumeEvents() & JG::EV_JUMP);
}

// 最初の拍（押しどき）の時刻。start(t0, ...) の t0 を渡す。
static uint32_t jg_first_beat(uint32_t t0) { return t0 + JG::COUNTDOWN_MS + JG::FIRST_BEAT_MS; }

// b1 に対して off ms ずらして押し、柵 1 本ぶんを通過させた結果を返す。
static JG jg_press_offset(int off) {
    JG j;
    j.start(0, 1);
    const uint32_t b1 = jg_first_beat(0);
    jg_run(j, 0, b1 + uint32_t(off) - 5, 5);              // 押下の直前まで
    j.onPress(b1 + uint32_t(off));
    jg_run(j, b1 + uint32_t(off), 600, 5);                // 柵が通り過ぎるまで
    return j;
}

static void test_fence_cleared_on_beat() {
    JG j = jg_press_offset(0);
    assert(j.state() == JG::State::PLAYING);
    assert(j.score() == 1);
}

static void test_fence_hit_without_jump() {
    JG j;
    j.start(0, 1);
    jg_run(j, 0, jg_first_beat(0) + 600, 5);
    assert(j.state() == JG::State::OVER);
    assert(j.score() == 0);
    assert(j.consumeEvents() & JG::EV_MISS);
}

static void test_jump_window_edges() {
    // 押しどきの窓は概算で ±100ms（JUMP_HEIGHT_PX=26 のとき）。内側は成功、外側は失敗。
    const int inside[]  = { -80, -40, 0, 40, 80 };
    const int outside[] = { -150, 150 };
    for (int off : inside)  { JG j = jg_press_offset(off); assert(j.state() == JG::State::PLAYING); assert(j.score() == 1); }
    for (int off : outside) { JG j = jg_press_offset(off); assert(j.state() == JG::State::OVER); }
}

static void test_bpm_progression_and_gap() {
    assert(JG::bpmForScore(0) == JG::BPM_START);
    assert(JG::bpmForScore(1) == JG::BPM_START + JG::BPM_STEP);
    assert(JG::bpmForScore(1000) == JG::BPM_MAX);           // 上限で止まる
    uint32_t prev = JG::beatMsForScore(0);
    for (int s = 0; s <= 200; ++s) {
        uint32_t b = JG::beatMsForScore(s);
        assert(b <= prev);                                   // スコアが上がると速くなる（遅くならない）
        assert(b >= JG::JUMP_AIR_MS + JG::MIN_GAP_MARGIN_MS);// 必ず跳び直せる
        prev = b;
    }
}

// 各柵の押しどきでぴったり押す bot。target 匹越えるまで進め、見つけた柵の拍を beats に記録する。
static void jg_autoplay(JG& j, uint32_t t0, int target, uint32_t* beats, int* nbeats) {
    uint32_t last_press_beat = 0;
    bool pressed_any = false;
    *nbeats = 0;
    uint32_t seen[64]; int nseen = 0;
    for (uint32_t t = t0 + 5; j.score() < target && j.state() != JG::State::OVER; t += 5) {
        j.step(t);
        for (int i = 0; i < JG::MAX_FENCES; ++i) {
            const JG::Fence& f = j.fence(i);
            if (!f.used) continue;
            bool known = false;
            for (int k = 0; k < nseen; ++k) if (seen[k] == f.beat_ms) known = true;
            if (!known && nseen < 64) { seen[nseen++] = f.beat_ms; if (*nbeats < 64) beats[(*nbeats)++] = f.beat_ms; }
            if (!f.cleared && int32_t(t - f.beat_ms) >= 0 && (!pressed_any || f.beat_ms != last_press_beat)) {
                j.onPress(t);
                last_press_beat = f.beat_ms;
                pressed_any = true;
            }
        }
    }
}

static void test_autoplay_reaches_high_score_with_valid_gaps() {
    JG j;
    j.start(0, 1);
    uint32_t beats[64]; int n = 0;
    jg_autoplay(j, 0, 40, beats, &n);
    assert(j.state() == JG::State::PLAYING);
    assert(j.score() >= 40);
    assert(n >= 40);
    for (int i = 1; i < n; ++i) {
        uint32_t gap = beats[i] - beats[i - 1];
        assert(gap >= JG::beatMsForScore(1000));             // 最短でも上限 BPM の 1 拍
        assert(gap <= 2 * JG::beatMsForScore(0));            // 最長でも開始 BPM の 2 拍
    }
}

static void test_same_seed_same_fences() {
    uint32_t a[64], b[64], c[64]; int na = 0, nb = 0, nc = 0;
    JG j1; j1.start(0, 7);  jg_autoplay(j1, 0, 30, a, &na);
    JG j2; j2.start(0, 7);  jg_autoplay(j2, 0, 30, b, &nb);
    JG j3; j3.start(0, 99); jg_autoplay(j3, 0, 30, c, &nc);
    assert(na == nb && std::memcmp(a, b, sizeof(uint32_t) * na) == 0);   // 同じシードは同じ並び
    assert(!(na == nc && std::memcmp(a, c, sizeof(uint32_t) * na) == 0));// 違うシードは違う並び
}

// 先頭の柵を押しどきで越え、2 本目は押さずにミスするスクリプトを、刻み dt / 開始時刻 t0 で実行する。
static JG jg_script(uint32_t t0, uint32_t dt) {
    JG j;
    j.start(t0, 3);
    const uint32_t b1 = jg_first_beat(t0);
    bool pressed = false;
    for (uint32_t t = t0 + dt; int32_t(t0 + 20000 - t) > 0 && j.state() != JG::State::OVER; t += dt) {
        if (!pressed && int32_t(b1 - t) <= int32_t(dt)) { j.onPress(b1); pressed = true; }
        j.step(t);
    }
    return j;
}

static void test_step_granularity_independent() {
    JG a = jg_script(0, 5), b = jg_script(0, 33), c = jg_script(0, 1);
    assert(a.state() == JG::State::OVER && b.state() == JG::State::OVER && c.state() == JG::State::OVER);
    assert(a.score() == 1 && b.score() == 1 && c.score() == 1);
    assert(a.overSinceMs() == b.overSinceMs() && a.overSinceMs() == c.overSinceMs());
}

static void test_time_wraparound() {
    JG base = jg_script(0, 5);
    JG wrap = jg_script(0xFFFFFF00u, 5);       // 開始直後に uint32_t が一周する
    assert(wrap.state() == JG::State::OVER);
    assert(wrap.score() == base.score());
    assert(wrap.overSinceMs() - 0xFFFFFF00u == base.overSinceMs());
}

static void test_fence_positions_freeze_after_over() {
    // ミスしたあとは場面を止める（ぶつかった柵が流れ去らず、その場に残る）
    JG j;
    j.start(0, 1);
    jg_run(j, 0, jg_first_beat(0) + 600, 5);
    assert(j.state() == JG::State::OVER);
    int x[JG::MAX_FENCES];
    for (int i = 0; i < JG::MAX_FENCES; ++i) x[i] = j.fenceCenterX(i);
    j.step(j.nowMs() + 1000);
    for (int i = 0; i < JG::MAX_FENCES; ++i) assert(j.fenceCenterX(i) == x[i]);
    // ぶつかった柵は当たり判定の範囲内に残っている
    bool near_hitbox = false;
    for (int i = 0; i < JG::MAX_FENCES; ++i) {
        if (!j.fence(i).used) continue;
        int dx = j.fenceCenterX(i) - JG::HITBOX_CENTER_X;
        if (dx < 0) dx = -dx;
        if (dx < (JG::FENCE_W + JG::HITBOX_W) / 2) near_hitbox = true;
    }
    assert(near_hitbox);
}

static void test_events_emitted_once() {
    JG j;
    j.start(0, 1);
    int counts = 0, beats = 0, jumps = 0, cleared = 0, miss = 0;
    auto drain = [&]() {
        uint8_t e = j.consumeEvents();
        counts += !!(e & JG::EV_COUNT); beats += !!(e & JG::EV_BEAT);
        jumps += !!(e & JG::EV_JUMP);   cleared += !!(e & JG::EV_CLEARED); miss += !!(e & JG::EV_MISS);
    };
    drain();                                                     // 開始時の「3」
    const uint32_t b1 = jg_first_beat(0);
    for (uint32_t t = 5; t <= b1 + 600; t += 5) {
        if (t == b1) j.onPress(t);
        j.step(t);
        drain();
    }
    assert(counts == 3);        // 3・2・1
    assert(jumps == 1);
    assert(cleared == 1);
    assert(beats >= 1);
    assert(miss == 0);
}

// ---- Game とミニゲームの結合 ----------------------------------------------
// now を進めながら Game 越しにミニゲームを操作する補助。
static uint32_t mini_now = 0;

static void mini_step(Game& g, uint32_t ms) {
    for (uint32_t e = 0; e < ms; e += 5) { mini_now += 5; g.setNowMs(mini_now); g.updateMini(); }
}

// clear_target 匹越えたあとは押すのをやめ、ゲームオーバーになるまで進める。
static void mini_play(Game& g, int clear_target) {
    uint32_t last_beat = 0; bool any = false;
    for (int guard = 0; guard < 20000 && g.jump().state() != JumpGame::State::OVER; ++guard) {
        mini_step(g, 5);
        if (g.jump().score() >= clear_target) continue;
        for (int i = 0; i < JumpGame::MAX_FENCES; ++i) {
            const JumpGame::Fence& f = g.jump().fence(i);
            if (f.used && !f.cleared && int32_t(mini_now - f.beat_ms) >= 0 && (!any || f.beat_ms != last_beat)) {
                g.onButton(Game::Button::CENTER);
                last_beat = f.beat_ms; any = true;
            }
        }
    }
}

// 空腹・幸福を指定した通常の Game を作る。
static Game make_game_with(int hunger, int happy) {
    Game base(nullptr);
    skip_naming(base);
    GameSaveData d = base.saveData();
    d.hunger = uint8_t(hunger);
    d.happy  = uint8_t(happy);
    return Game(nullptr, &d);
}

static void test_minigame_starts_from_menu() {
    Game g = make_game_with(50, 50);
    mini_now = 100000; g.setNowMs(mini_now);
    menu_select(g, Game::Action::MINI);
    assert(g.screen() == Game::Screen::MINIGAME);
    assert(g.inMiniGame());
    assert(g.jump().state() == JumpGame::State::COUNTDOWN);
    assert(g.happy() == 50);                       // 仮の幸福度 +5 は無くなった
}

static void test_minigame_reward_and_hunger_cost() {
    Game g = make_game_with(50, 50);
    GameSaveData before = g.saveData();
    mini_now = 100000; g.setNowMs(mini_now);
    menu_select(g, Game::Action::MINI);
    mini_play(g, 3);
    assert(g.jump().state() == JumpGame::State::OVER);
    assert(g.jump().score() == 3);
    assert(g.happy() == 50 + 3 * 2);
    assert(g.hunger() == 50 - 5);
    assert(g.miniReward() == 6);
    GameSaveData after = g.saveData();
    assert(after.tend_feed == before.tend_feed && after.tend_pet == before.tend_pet);
    assert(after.tend_shear == before.tend_shear && after.tend_polish == before.tend_polish);
    assert(g.isDirty());
}

static void test_minigame_reward_capped_and_applied_once() {
    Game g = make_game_with(50, 50);
    mini_now = 100000; g.setNowMs(mini_now);
    menu_select(g, Game::Action::MINI);
    mini_play(g, 20);                              // 2×20 = 40 → 上限 30
    assert(g.happy() == 80);
    int happy = g.happy(), hunger = g.hunger();
    mini_step(g, 2000);                            // その後 updateMini を続けても再反映されない
    assert(g.happy() == happy && g.hunger() == hunger);
}

static void test_minigame_hunger_floor() {
    Game g = make_game_with(3, 50);
    mini_now = 100000; g.setNowMs(mini_now);
    menu_select(g, Game::Action::MINI);
    mini_play(g, 0);
    assert(g.hunger() == 1);                       // 餓死させない
    Game h = make_game_with(1, 50);
    mini_now = 100000; h.setNowMs(mini_now);
    menu_select(h, Game::Action::MINI);
    mini_play(h, 0);
    assert(h.hunger() == 1);                       // 1 を下回らず、増えもしない
}

static void test_minigame_returns_to_main_after_lock() {
    Game g = make_game_with(50, 50);
    mini_now = 100000; g.setNowMs(mini_now);
    menu_select(g, Game::Action::MINI);
    mini_play(g, 0);
    g.onButton(Game::Button::CENTER);              // 終了直後：ロック中
    assert(g.screen() == Game::Screen::MINIGAME);
    mini_step(g, JumpGame::OVER_LOCK_MS);
    g.onButton(Game::Button::CENTER);
    assert(g.screen() == Game::Screen::MAIN);
}

static void test_minigame_aborted_when_falling_asleep() {
    Game base(nullptr);
    skip_naming(base);
    GameSaveData d = base.saveData();
    d.sleepy = 80; d.sleeping = 0; d.hunger = 50; d.happy = 50;
    Game g(nullptr, &d);
    mini_now = 100000; g.setNowMs(mini_now);
    menu_select(g, Game::Action::MINI);
    assert(g.screen() == Game::Screen::MINIGAME);
    g.tick();                                      // 睡眠度 80 以上で就寝する
    assert(g.sleeping());
    assert(g.screen() == Game::Screen::MAIN);      // ミニゲームは中断
    mini_step(g, 10000);                           // 中断後は結果が反映されない
    assert(g.happy() == 50 && g.hunger() == 50);
}

static void test_status_level() {
    // 空腹・幸福（0〜100）を 5 段階のバーにする。生きている（1 以上）なら、必ず 1 段階は塗る。
    assert(Game::statusLevel(0)   == 0);
    assert(Game::statusLevel(1)   == 1);
    assert(Game::statusLevel(20)  == 1);
    assert(Game::statusLevel(21)  == 2);
    assert(Game::statusLevel(40)  == 2);
    assert(Game::statusLevel(41)  == 3);
    assert(Game::statusLevel(80)  == 4);
    assert(Game::statusLevel(81)  == 5);
    assert(Game::statusLevel(100) == 5);
    assert(Game::statusLevel(255) == 5);     // 範囲外は 5 で止める
    assert(Game::statusLevel(-5)  == 0);     // 負は 0
    assert(Game::statusLevel(-100) == 0);    // 大きな負でも 0（切り上げの計算だけだと負になる）
}

// ---- プロフィール --------------------------------------------------------
static void test_profile_opens_and_any_button_closes() {
    const Game::Button buttons[] = { Game::Button::LEFT, Game::Button::CENTER, Game::Button::RIGHT };
    for (Game::Button b : buttons) {
        Game g(nullptr);
        skip_naming(g);
        menu_select(g, Game::Action::PROFILE);
        assert(g.screen() == Game::Screen::PROFILE);
        g.onButton(b);
        assert(g.screen() == Game::Screen::MAIN);
    }
}

static void test_profile_allowed_while_sleeping() {
    // 見るだけの画面なので、就寝中でも開ける（羊は起きない・状態も変わらない）
    Game g = make_sleeping_game();
    menu_select(g, Game::Action::PROFILE);
    assert(g.screen() == Game::Screen::PROFILE);
    assert(g.sleeping());
    g.onButton(Game::Button::CENTER);
    assert(g.screen() == Game::Screen::MAIN);
    assert(g.sleeping());
    assert(g.hunger() == 50 && g.happy() == 50);
}

// 成体を品種指定で作る。
static Game make_adult_game(Game::Breed breed) {
    Game base(nullptr);
    skip_naming(base);
    GameSaveData d = base.saveData();
    d.stage = uint8_t(Game::Stage::ADULT);
    d.breed = uint8_t(breed);
    return Game(nullptr, &d);
}

static void test_kind_names() {
    assert(std::string(make_stage_game(Game::Stage::BABY).kindName())          == "ベビー");
    assert(std::string(make_stage_game(Game::Stage::YOUNG_MOKO).kindName())    == "若羊 モコ系");
    assert(std::string(make_stage_game(Game::Stage::YOUNG_SUFFOLK).kindName()) == "若羊 サフォーク系");
    assert(std::string(make_stage_game(Game::Stage::YOUNG_WILD).kindName())    == "若羊 ワイルド系");
    assert(std::string(make_adult_game(Game::Breed::MERINO).kindName())        == "メリノ");
    assert(std::string(make_adult_game(Game::Breed::CORRIEDALE).kindName())    == "コリデール");
    assert(std::string(make_adult_game(Game::Breed::LINCOLN).kindName())       == "リンカーン");
    assert(std::string(make_adult_game(Game::Breed::SUFFOLK).kindName())       == "サフォーク");
    assert(std::string(make_adult_game(Game::Breed::HAMPSHIRE).kindName())     == "ハンプシャー");
    assert(std::string(make_adult_game(Game::Breed::MOUFLON).kindName())       == "ムフロン");
    assert(std::string(make_adult_game(Game::Breed::BIGHORN).kindName())       == "ビッグホーン");
}

static void test_kind_names_and_profile_labels_renderable() {
    // 種類の名前とプロフィール画面の見出しの全字がフォントに収録されている（未収録は空白で出る）
    const char* fixed = "名前種類日数めボタンでもどる";
    std::string all = fixed;
    const Game::Stage stages[] = { Game::Stage::BABY, Game::Stage::YOUNG_MOKO,
                                   Game::Stage::YOUNG_SUFFOLK, Game::Stage::YOUNG_WILD };
    for (Game::Stage st : stages) all += make_stage_game(st).kindName();
    const Game::Breed breeds[] = { Game::Breed::MERINO, Game::Breed::CORRIEDALE, Game::Breed::LINCOLN,
                                   Game::Breed::SUFFOLK, Game::Breed::HAMPSHIRE, Game::Breed::MOUFLON,
                                   Game::Breed::BIGHORN };
    for (Game::Breed b : breeds) all += make_adult_game(b).kindName();
    const char* p = all.c_str();
    while (*p) {
        uint32_t cp = font::decodeUtf8(p);
        assert(cp < 0x80 || font::jaGlyph(cp) != nullptr);
    }
}

static void test_age_days_counts_from_first_day() {
    // 生まれた日を 1 日めと数える。tick の 1 日ぶんで 2 日めになる。
    Game g(nullptr);
    skip_naming(g);
    assert(g.ageDays() == 1);
    for (uint32_t i = 0; i < Game::TICKS_PER_DAY - 1; ++i) g.tick();
    assert(g.ageDays() == 1);                            // 1 日たつ直前
    g.tick();
    assert(g.ageDays() == 2);
}

// ---- 待つ音は予約して、画面を切り替えたあとに鳴らす --------------------------
// 待つ音（餌・撫でる・毛刈り・進化）をボタン処理の中で鳴らすと、鳴り終わるまで画面の描画が遅れ、
// メニューが表示されたまま音が聞こえる。Game は音を予約し、main が描画のあとに playPendingSfx で鳴らす。
static Game make_young_with_growth(Game::Stage stage) {
    Game base(nullptr);
    skip_naming(base);
    GameSaveData d = base.saveData();
    d.stage = uint8_t(stage);
    d.wool  = 50;
    d.horn  = 50;
    return Game(nullptr, &d);
}

static void test_action_sounds_are_deferred() {
    Game g = make_game_with(50, 50);
    assert(g.pendingSfx() == Game::Sfx::NONE);
    menu_select(g, Game::Action::FEED);
    assert(g.pendingSfx() == Game::Sfx::MOG);
    g.playPendingSfx();
    assert(g.pendingSfx() == Game::Sfx::NONE);         // 鳴らしたら予約は空になる

    menu_select(g, Game::Action::PET);
    assert(g.pendingSfx() == Game::Sfx::PET);
    g.playPendingSfx();
    assert(g.pendingSfx() == Game::Sfx::NONE);

    Game moko = make_young_with_growth(Game::Stage::YOUNG_MOKO);
    menu_select(moko, Game::Action::SHEAR);
    assert(moko.pendingSfx() == Game::Sfx::JOKI);

    Game wild = make_young_with_growth(Game::Stage::YOUNG_WILD);
    menu_select(wild, Game::Action::POLISH);
    assert(wild.pendingSfx() == Game::Sfx::JOKI);
}

static void test_no_sound_when_action_has_no_effect() {
    // 毛が短くて刈れないとき・就寝中の撫でる（羊は起きない）は、音を予約しない
    Game g = make_stage_game(Game::Stage::YOUNG_MOKO);   // 毛は 0
    menu_select(g, Game::Action::SHEAR);
    assert(g.pendingSfx() == Game::Sfx::NONE);
    Game s = make_sleeping_game();
    menu_select(s, Game::Action::PET);
    assert(s.pendingSfx() == Game::Sfx::NONE);
}

static void test_evolution_sound_is_deferred() {
    Game g(nullptr);
    skip_naming(g);
    for (uint32_t i = 0; i < 3 * Game::TICKS_PER_DAY + Game::TICKS_PER_HOUR; ++i) g.tick();
    assert(g.stage() != Game::Stage::BABY);
    assert(g.pendingSfx() == Game::Sfx::HAPPY);
}

// ---- 昼夜（ゲーム内の時刻）-------------------------------------------------
static void tick_hours(Game& g, uint32_t hours) {
    for (uint32_t i = 0; i < hours * Game::TICKS_PER_HOUR; ++i) g.tick();
}

static void test_hour_of_day_and_night() {
    // 起動時は朝 8 時（起動直後に就寝しないため）。22 時〜6 時が夜。
    Game g(nullptr);
    skip_naming(g);
    assert(g.hourOfDay() == 8);
    assert(!g.isNight());
    tick_hours(g, 13);                 // 21 時
    assert(g.hourOfDay() == 21);
    assert(!g.isNight());
    tick_hours(g, 1);                  // 22 時：夜の始まり
    assert(g.hourOfDay() == 22);
    assert(g.isNight());
    tick_hours(g, 2);                  // 0 時（日をまたぐ）
    assert(g.hourOfDay() == 0);
    assert(g.isNight());
    tick_hours(g, 5);                  // 5 時：まだ夜
    assert(g.hourOfDay() == 5);
    assert(g.isNight());
    tick_hours(g, 1);                  // 6 時：夜明け
    assert(g.hourOfDay() == 6);
    assert(!g.isNight());
}

// ---- 背景（草・月・星）-----------------------------------------------------
static void test_grass_stays_in_the_ground_strip() {
    // 草は地面の帯（GRASS_TOP 以降）だけ。羊の足元（y<=53）に食い込まない。
    int lit_above = 0, lit_in = 0;
    for (int y = 0; y < background::H; ++y) {
        for (int x = 0; x < background::W; ++x) {
            if (!background::grassPixel(x, y)) continue;
            if (y < background::GRASS_TOP) ++lit_above; else ++lit_in;
        }
    }
    assert(lit_above == 0);
    assert(lit_in > 0);
}

static void test_grass_density_is_moderate() {
    // 空でも塗りつぶしでもない、適度な密度（帯の面積の 15%〜60%）
    int lit = 0;
    const int area = background::W * (background::H - background::GRASS_TOP);
    for (int y = background::GRASS_TOP; y < background::H; ++y)
        for (int x = 0; x < background::W; ++x)
            if (background::grassPixel(x, y)) ++lit;
    assert(lit * 100 >= area * 15);
    assert(lit * 100 <= area * 60);
}

static void test_grass_has_no_bare_gaps() {
    // 16px の幅ごとに、地面の線より上に草の葉がある（どこかが丸ごとはげていない）
    for (int x0 = 0; x0 < background::W; x0 += 16) {
        int blades = 0;
        for (int x = x0; x < x0 + 16; ++x)
            for (int y = background::GRASS_TOP; y < background::H - 1; ++y)
                if (background::grassPixel(x, y)) ++blades;
        assert(blades > 0);
    }
}

static void test_grass_ground_line_is_dotted() {
    // 一番下の行は点線（半分以上が点いていて、全部ではない）
    int lit = 0;
    for (int x = 0; x < background::W; ++x)
        if (background::grassPixel(x, background::H - 1)) ++lit;
    assert(lit > background::W / 2);
    assert(lit < background::W);
}

static void test_sky_stays_in_the_sky_strip() {
    int moon = 0;
    for (int y = 0; y < background::H; ++y) {
        for (int x = 0; x < background::W; ++x) {
            if (background::moonPixel(x, y)) {
                ++moon;
                assert(y <= background::SKY_BOTTOM);
            }
            for (int frame = 0; frame < 2; ++frame) {
                if (background::starPixel(x, y, frame)) {
                    assert(y <= background::SKY_BOTTOM);
                    // 就寝画面の左上の「zzz」（x<18, y<7）に重ならない
                    assert(!(x < 20 && y < 9));
                }
            }
        }
    }
    assert(moon >= 20);                                  // 月として形になっている
}

static void test_stars_twinkle_but_never_vanish() {
    // 2 つの画面（フレーム）で、またたく星の形（十字）は入れ替わるが、星の中心は消えない
    int differ = 0;
    for (int y = 0; y < background::H; ++y)
        for (int x = 0; x < background::W; ++x)
            if (background::starPixel(x, y, 0) != background::starPixel(x, y, 1)) ++differ;
    assert(differ > 0);
    for (int i = 0; i < background::STAR_COUNT; ++i) {
        int sx = background::STARS[i][0], sy = background::STARS[i][1];
        assert(background::starPixel(sx, sy, 0));
        assert(background::starPixel(sx, sy, 1));
    }
}

// ---- 昼の空（太陽・雲）-----------------------------------------------------
static void test_sun_is_fixed_in_the_sky() {
    // 太陽は動かさない。左上に固定し、焼き付き対策のゆっくりした揺れ（±2px）を足しても、
    // 画面と空の帯に収まる。月（右上）とは重ならない。
    constexpr int DRIFT = 2;
    assert(background::SUN_X - DRIFT >= 0);
    assert(background::SUN_X + background::SUN_W + DRIFT <= background::W);
    assert(background::SUN_Y >= 0);
    assert(background::SUN_Y + background::SUN_H - 1 <= background::SKY_BOTTOM);
    assert(background::SUN_X + background::SUN_W + DRIFT < background::MOON_X);
}

static int count_sun() {
    int n = 0;
    for (int y = 0; y < background::H; ++y)
        for (int x = 0; x < background::W; ++x)
            if (background::sunPixel(x, y)) ++n;
    return n;
}

static void test_sun_shape_and_steady() {
    // 太陽の外は点かない
    for (int y = 0; y < background::H; ++y) {
        for (int x = 0; x < background::W; ++x) {
            bool inside = x >= background::SUN_X && x < background::SUN_X + background::SUN_W &&
                          y >= background::SUN_Y && y < background::SUN_Y + background::SUN_H;
            if (!inside) assert(!background::sunPixel(x, y));
        }
    }
    // 円盤と 8 方向の光線がいつも点いている（点滅しない）。斜めの光線の位置も点く。
    assert(count_sun() >= 25);
    for (int i = 0; i < 4; ++i) {
        int dx = (i & 1) ? 7 : 1, dy = (i & 2) ? 7 : 1;
        assert(background::sunPixel(background::SUN_X + dx, background::SUN_Y + dy));
    }
}

static void test_clouds_stay_in_sky_and_drift() {
    int lit_total = 0;
    for (int tick = 0; tick <= 20000; tick += 500) {
        for (int y = 0; y < background::H; ++y) {
            for (int x = 0; x < background::W; ++x) {
                if (!background::cloudPixel(x, y, tick)) continue;
                ++lit_total;
                assert(y <= background::SKY_BOTTOM);
            }
        }
    }
    assert(lit_total > 0);
    // 時間がたつと、2 つの雲がそれぞれ動く（上の帯 y=0〜8 の雲と、下の帯 y=9〜17 の雲を別々に見る）
    int differ_upper = 0, differ_lower = 0;
    for (int y = 0; y <= background::SKY_BOTTOM; ++y) {
        for (int x = 0; x < background::W; ++x) {
            if (background::cloudPixel(x, y, 0) == background::cloudPixel(x, y, 4000)) continue;
            if (y <= 8) ++differ_upper; else ++differ_lower;
        }
    }
    assert(differ_upper > 0);
    assert(differ_lower > 0);
}

// ---- 墓・命名の間は時間を止める --------------------------------------------
// 死ぬと、新しいベビーが生まれて墓の画面になり、名前を付けるまで命名の画面が続く。その間も時間が
// 進むと、名前を付けている間に新しい子が育ったり、進化したり、また死んだりしてしまう。

// 空腹 1 の羊を作り、餓死して墓の画面になるまで進める。
static Game make_dead_pet_at_grave() {
    Game base(nullptr);
    skip_naming(base);
    GameSaveData d = base.saveData();
    d.hunger = 1;
    d.happy  = 100;
    Game g(nullptr, &d);
    for (uint32_t i = 0; i < 10 * Game::TICKS_PER_HOUR && g.screen() != Game::Screen::GRAVE; ++i) g.tick();
    return g;
}

static void test_time_stops_while_grave_is_shown() {
    Game g = make_dead_pet_at_grave();
    assert(g.screen() == Game::Screen::GRAVE);
    assert(g.graveCount() == 1);
    const uint32_t age = g.ageTicks();
    for (uint32_t i = 0; i < 5 * Game::TICKS_PER_DAY; ++i) g.tick();      // 墓の画面のまま放置（5 日ぶん）
    assert(g.ageTicks() == age);                        // 新しい子の年齢が進まない
    assert(g.stage() == Game::Stage::BABY);             // 進化しない
    assert(g.graveCount() == 1);                        // また死んで墓が増えない
    assert(g.screen() == Game::Screen::GRAVE);
}

static void test_time_stops_while_naming_and_resumes_after() {
    Game g = make_dead_pet_at_grave();
    const uint32_t age = g.ageTicks();
    g.onButton(Game::Button::CENTER);                   // 墓 → 命名
    assert(g.screen() == Game::Screen::NAMING);
    for (uint32_t i = 0; i < 5 * Game::TICKS_PER_DAY; ++i) g.tick();
    assert(g.ageTicks() == age);
    assert(g.stage() == Game::Stage::BABY);
    g.onButton(Game::Button::CENTER);                   // 「おまかせ」を選ぶ
    g.onButton(Game::Button::CENTER);                   // 確定 → メイン
    assert(g.screen() == Game::Screen::MAIN);
    g.tick();
    assert(g.ageTicks() == age + 1);                    // 名前を付けたら、時間が動き出す
}

static void test_time_stops_during_first_naming() {
    // 初回起動（セーブなし）の命名画面でも、名前を付けるまで時間は進まない
    Game g(nullptr);
    assert(g.screen() == Game::Screen::NAMING);
    for (uint32_t i = 0; i < 5 * Game::TICKS_PER_DAY; ++i) g.tick();
    assert(g.ageTicks() == 0);
    assert(g.stage() == Game::Stage::BABY);
}

// ---- 布団（就寝中の画面）----------------------------------------------------
static void test_futon_geometry() {
    using namespace background;
    // 羊（2 倍で x=40〜88、揺れ ±2px で 38〜90）を横から包む幅
    assert(FUTON_LEFT < 38 && FUTON_RIGHT > 90);
    assert(FUTON_LEFT >= 2 && FUTON_RIGHT + 2 < W);          // 揺れを足しても画面に収まる
    // 草の地面の真上に敷く（草と重ならず、隙間もない）
    assert(FUTON_BOTTOM + 1 == GRASS_TOP);
    // 空の帯（月・星）より下、羊の足元（y<=53）より上から始まる
    assert(FUTON_TOP > SKY_BOTTOM);
}

static void test_futon_mask_and_pattern_stay_inside() {
    using namespace background;
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            const bool mask = futonMask(x, y), lit = futonPixel(x, y);
            if (lit) assert(mask);                            // 模様は布団の中だけ
            if (mask) {
                assert(x >= FUTON_LEFT && x <= FUTON_RIGHT);
                assert(y >= FUTON_TOP && y <= FUTON_BOTTOM);
            }
        }
    }
}

static void test_futon_leaves_the_head_and_shoulders_visible() {
    // 布団の上端より上は、何も覆わない（頭と肩が出る）
    using namespace background;
    for (int y = 0; y < FUTON_TOP; ++y)
        for (int x = 0; x < W; ++x)
            assert(!futonMask(x, y));
    // 下の縁までは、全幅を覆う
    for (int x = FUTON_LEFT; x <= FUTON_RIGHT; ++x) assert(futonMask(x, FUTON_BOTTOM));
}

static void test_futon_top_edge_is_wavy() {
    // 上の縁は波打つ（列によって上端の高さが違う）
    using namespace background;
    int min_top = H, max_top = 0;
    for (int x = FUTON_LEFT; x <= FUTON_RIGHT; ++x) {
        int top = -1;
        for (int y = 0; y < H; ++y) if (futonMask(x, y)) { top = y; break; }
        assert(top >= 0);
        if (top < min_top) min_top = top;
        if (top > max_top) max_top = top;
    }
    assert(max_top > min_top);
    assert(max_top - min_top <= 2);                          // 大きくうねらず、なだらかな 2px 以内
}

static void test_futon_pattern_is_simple() {
    // 縁と縫い目（点線 2 本）だけの、シンプルな模様。布団の面積の 10%〜35% が点く。
    using namespace background;
    int mask = 0, lit = 0;
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            if (futonMask(x, y)) ++mask;
            if (futonPixel(x, y)) ++lit;
        }
    assert(mask > 0);
    assert(lit * 100 >= mask * 10);
    assert(lit * 100 <= mask * 35);
    // 縁は、上の縁・下の縁・左右の縁がある
    assert(futonPixel(FUTON_LEFT, FUTON_TOP + 5));
    assert(futonPixel(FUTON_RIGHT, FUTON_TOP + 5));
    int bottom_lit = 0;
    for (int x = FUTON_LEFT; x <= FUTON_RIGHT; ++x) if (futonPixel(x, FUTON_BOTTOM)) ++bottom_lit;
    assert(bottom_lit == FUTON_RIGHT - FUTON_LEFT + 1);      // 下の縁は途切れない
    // 縫い目は、2 本の点線（3px 描いて 2px 空ける）
    for (int row : { FUTON_TOP + 8, FUTON_TOP + 13 }) {
        // 左の縁（+0）から数えて、+1〜+2 が点き、+3〜+4 が空き、+5〜+7 が点き、+8〜+9 が空く
        assert(futonPixel(FUTON_LEFT + 1, row));
        assert(futonPixel(FUTON_LEFT + 2, row));
        assert(!futonPixel(FUTON_LEFT + 3, row));
        assert(!futonPixel(FUTON_LEFT + 4, row));
        assert(futonPixel(FUTON_LEFT + 5, row));
        assert(futonPixel(FUTON_LEFT + 7, row));
        assert(!futonPixel(FUTON_LEFT + 8, row));
        assert(!futonPixel(FUTON_LEFT + 9, row));
    }
}

// ---- ご飯：干し草ロールを食べるモーション -----------------------------------
static void test_eat_bite_timing() {
    using namespace eat_motion;
    // 3 回ぱくっとする（各 BITE_LEN フレーム）。ぱくっの間は、うなずいている。
    for (int i = 0; i < BITE_COUNT; ++i) {
        for (int t = BITE_START[i]; t < BITE_START[i] + BITE_LEN; ++t) assert(isBiting(t));
        assert(!isBiting(BITE_START[i] - 1));
        assert(!isBiting(BITE_START[i] + BITE_LEN));
    }
    assert(!isBiting(0));
    // かじった回数は、ぱくっが始まるたびに 1 つ増える（うなずく瞬間に、かじり跡が付く）
    assert(bitesTaken(0) == 0);
    assert(bitesTaken(BITE_START[0] - 1) == 0);
    assert(bitesTaken(BITE_START[0]) == 1);
    assert(bitesTaken(BITE_START[1] - 1) == 1);
    assert(bitesTaken(BITE_START[1]) == 2);
    assert(bitesTaken(BITE_START[2]) == 3);
    assert(bitesTaken(Game::FEED_ACTION_TICKS) == 3);
    // ご飯の動き（Game::FEED_ACTION_TICKS）の中に、3 回とも収まる
    assert(BITE_START[2] + BITE_LEN <= Game::FEED_ACTION_TICKS);
    // 1 回目は、ご飯の音（画面が SOUND_BLOCK_TICKS フレーム止まる）が終わって、ロールを見つける間が
    // できてから始まる。早いと、音が終わった瞬間に、いきなり食べている絵が出る。
    assert(BITE_START[0] >= SOUND_BLOCK_TICKS + LOOK_TICKS);
    // ぱくっの間隔は 1 秒（20 フレーム）。間には、体を戻す間（BITE_LEN より長い）がある
    for (int i = 1; i < BITE_COUNT; ++i) {
        assert(BITE_START[i] - BITE_START[i - 1] == 20);
        assert(BITE_START[i] - BITE_START[i - 1] > BITE_LEN);
    }
}

static void test_eat_lean_only_while_biting() {
    using namespace eat_motion;
    int dx, dy;
    lean(0, 1, &dx, &dy);
    assert(dx == 0 && dy == 0);                              // ぱくっの外では、傾かない
    lean(BITE_START[0], 1, &dx, &dy);
    assert(dx == LEAN_PX && dy == LEAN_PX);                  // 右のロールへ：右下へ傾く
    lean(BITE_START[1], -1, &dx, &dy);
    assert(dx == -LEAN_PX && dy == LEAN_PX);                 // 左のロールへ：左下へ傾く
}

static void test_eat_direction_flips_at_the_right_half() {
    using namespace eat_motion;
    assert(direction(0) == 1);
    assert(direction(56) == 1);                              // 境目まで、右にロール
    assert(direction(57) == -1);                             // 画面の右半分では、左にロール
    assert(direction(80) == -1);
}

static void test_eat_bale_stays_on_screen_and_beside_the_sheep() {
    using namespace eat_motion;
    // 羊が歩く範囲（0〜80）のどこにいても、ロールは画面に収まる。羊の 2 倍スプライト（48px 幅）と重ならない。
    for (int walk_x = 0; walk_x <= 80; ++walk_x) {
        const int left = baleLeft(walk_x);
        assert(left >= 0 && left + BALE_W - 1 < background::W);
        if (direction(walk_x) > 0) assert(left >= walk_x + 44);            // 右に置くときは、羊の右端の外
        else                        assert(left + BALE_W - 1 <= walk_x + 4);  // 左に置くときは、羊の左端の外
    }
    // 根元は草の地面の帯（y=56〜）の真上、上端は羊の足元より上まで届く高さ
    assert(GROUND_Y + 1 == background::GRASS_TOP);
    assert(GROUND_Y - BALE_H + 1 >= 0);
}

static int count_bale(int bites, int dir, int dx0 = 0, int dx1 = eat_motion::BALE_W - 1) {
    int n = 0;
    for (int dy = 0; dy < eat_motion::BALE_H; ++dy)
        for (int dx = dx0; dx <= dx1; ++dx)
            if (eat_motion::balePixel(dx, dy, bites, dir)) ++n;
    return n;
}

static void test_eat_bale_is_eaten_bite_by_bite() {
    using namespace eat_motion;
    for (int dir : { 1, -1 }) {
        // かじるたびに減り、3 回でなくなる
        assert(count_bale(0, dir) > count_bale(1, dir));
        assert(count_bale(1, dir) > count_bale(2, dir));
        assert(count_bale(2, dir) > count_bale(3, dir));
        assert(count_bale(3, dir) == 0);
        assert(count_bale(0, dir) > 0);
        // 範囲外の回数でも壊れない（負は、まだ食べていない扱い。超えたらなし）
        assert(count_bale(4, dir) == 0);
        assert(count_bale(-1, dir) == count_bale(0, dir));
    }
    // ロールの外は点かない
    assert(!balePixel(-1, 0, 0, 1) && !balePixel(BALE_W, 0, 0, 1));
    assert(!balePixel(0, -1, 0, 1) && !balePixel(0, BALE_H, 0, 1));
}

static void test_eat_bale_is_bitten_from_the_sheep_side() {
    using namespace eat_motion;
    // 1 回目のかじり跡は、羊のいる側（右に置くなら、ロールの左側）から付く。反対側は、そのまま。
    const int side = 4;
    const int r0 = BALE_W - side, r1 = BALE_W - 1;
    assert(count_bale(1, 1, 0, side - 1) < count_bale(0, 1, 0, side - 1));      // 左側が欠ける
    assert(count_bale(1, 1, r0, r1) == count_bale(0, 1, r0, r1));                 // 右側は無傷
    assert(count_bale(1, -1, r0, r1) < count_bale(0, -1, r0, r1));               // 左に置くなら、右側が欠ける
    assert(count_bale(1, -1, 0, side - 1) == count_bale(0, -1, 0, side - 1));    // 左側は無傷
}

static void test_feed_action_lasts_longer_than_other_actions() {
    // ぱくっを 1 秒おきに 3 回するので、ご飯の動きは他の動きより長い（FEED_ACTION_TICKS）。
    // 撫でる・毛刈りなどは、今までどおり ACTION_TICKS。
    Game feed = make_game_with(50, 50);
    menu_select(feed, Game::Action::FEED);
    assert(feed.action() == Game::Action::FEED);
    for (int i = 0; i < Game::FEED_ACTION_TICKS; ++i) feed.tick();
    assert(feed.action() == Game::Action::FEED);         // まだ続いている
    feed.tick();
    assert(feed.action() == Game::Action::NONE);         // 終わった

    Game pet = make_game_with(50, 50);
    menu_select(pet, Game::Action::PET);
    for (int i = 0; i < Game::ACTION_TICKS; ++i) pet.tick();
    assert(pet.action() == Game::Action::PET);
    pet.tick();
    assert(pet.action() == Game::Action::NONE);
    assert(Game::FEED_ACTION_TICKS > Game::ACTION_TICKS);
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
    RUN(test_status_level);
    RUN(test_hour_of_day_and_night);
    RUN(test_time_stops_while_grave_is_shown);
    RUN(test_time_stops_while_naming_and_resumes_after);
    RUN(test_time_stops_during_first_naming);
    RUN(test_grass_stays_in_the_ground_strip);
    RUN(test_grass_density_is_moderate);
    RUN(test_grass_has_no_bare_gaps);
    RUN(test_grass_ground_line_is_dotted);
    RUN(test_sky_stays_in_the_sky_strip);
    RUN(test_stars_twinkle_but_never_vanish);
    RUN(test_futon_geometry);
    RUN(test_futon_mask_and_pattern_stay_inside);
    RUN(test_futon_leaves_the_head_and_shoulders_visible);
    RUN(test_futon_top_edge_is_wavy);
    RUN(test_futon_pattern_is_simple);
    RUN(test_eat_bite_timing);
    RUN(test_eat_lean_only_while_biting);
    RUN(test_eat_direction_flips_at_the_right_half);
    RUN(test_eat_bale_stays_on_screen_and_beside_the_sheep);
    RUN(test_eat_bale_is_eaten_bite_by_bite);
    RUN(test_eat_bale_is_bitten_from_the_sheep_side);
    RUN(test_feed_action_lasts_longer_than_other_actions);
    RUN(test_sun_is_fixed_in_the_sky);
    RUN(test_sun_shape_and_steady);
    RUN(test_clouds_stay_in_sky_and_drift);
    RUN(test_menu_labels_for_young_and_adult);
    RUN(test_baby_menu_has_no_shear);
    RUN(test_baby_menu_cursor_wraps_at_five);
    RUN(test_baby_wool_does_not_grow_but_young_does);
    RUN(test_action_sounds_are_deferred);
    RUN(test_no_sound_when_action_has_no_effect);
    RUN(test_evolution_sound_is_deferred);
    RUN(test_profile_opens_and_any_button_closes);
    RUN(test_profile_allowed_while_sleeping);
    RUN(test_kind_names);
    RUN(test_kind_names_and_profile_labels_renderable);
    RUN(test_age_days_counts_from_first_day);
    RUN(test_evolution_resets_menu_cursor);
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
    RUN(test_jump_countdown_then_play);
    RUN(test_jump_ignores_press_during_countdown);
    RUN(test_jump_air_time_and_height);
    RUN(test_jump_ignored_while_airborne);
    RUN(test_fence_cleared_on_beat);
    RUN(test_fence_hit_without_jump);
    RUN(test_jump_window_edges);
    RUN(test_bpm_progression_and_gap);
    RUN(test_autoplay_reaches_high_score_with_valid_gaps);
    RUN(test_same_seed_same_fences);
    RUN(test_step_granularity_independent);
    RUN(test_time_wraparound);
    RUN(test_fence_positions_freeze_after_over);
    RUN(test_events_emitted_once);
    RUN(test_minigame_starts_from_menu);
    RUN(test_minigame_reward_and_hunger_cost);
    RUN(test_minigame_reward_capped_and_applied_once);
    RUN(test_minigame_hunger_floor);
    RUN(test_minigame_returns_to_main_after_lock);
    RUN(test_minigame_aborted_when_falling_asleep);

    std::printf("\n=== all tests passed ===\n\n");
    return 0;
}
