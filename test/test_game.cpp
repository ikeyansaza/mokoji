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
#include "melody.h"
#include "sound.h"
#include "pet_motion.h"
#include "shear_motion.h"
#include "polish_motion.h"
#include "sound.h"
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
static Game make_sleeping_game(Game::Stage stage = Game::Stage::BABY, Game::Breed breed = Game::Breed::NONE) {
    Game base(nullptr);
    skip_naming(base);
    GameSaveData d = base.saveData();
    d.stage    = uint8_t(stage);
    d.breed    = uint8_t(breed);
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
        // 毛刈りは成体のメニューにだけあるので、成体で確認する
        Game g = act == Game::Action::SHEAR
                     ? make_sleeping_game(Game::Stage::ADULT, Game::Breed::CORRIEDALE)
                     : make_sleeping_game(Game::Stage::BABY);
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

// 成体を品種指定で作る。
static Game make_adult_game(Game::Breed breed) {
    Game base(nullptr);
    skip_naming(base);
    GameSaveData d = base.saveData();
    d.stage = uint8_t(Game::Stage::ADULT);
    d.breed = uint8_t(breed);
    return Game(nullptr, &d);
}

// メニューラベル（UTF-8）。
static void test_menu_labels_for_young_and_adult() {
    // 毛刈り・角研ぎは成体だけ。ベビーと若羊のメニューには出ない（5 項目）。
    Game young = make_stage_game(Game::Stage::YOUNG_MOKO);
    assert(young.menuCount() == 5);
    assert(std::string(young.menuLabel(2)) == "ゲーム");
    assert(young.menuAction(2) == Game::Action::MINI);

    Game moko = make_adult_game(Game::Breed::CORRIEDALE);
    assert(moko.menuCount() == 6);
    assert(std::string(moko.menuLabel(0)) == "ごはん");    // FEED
    assert(std::string(moko.menuLabel(1)) == "なでる");    // PET
    assert(std::string(moko.menuLabel(2)) == "毛刈り");    // SHEAR
    assert(std::string(moko.menuLabel(3)) == "ゲーム");    // MINI
    assert(std::string(moko.menuLabel(4)) == "プロフ");    // PROFILE
    assert(std::string(moko.menuLabel(5)) == "もどる");    // BACK
    assert(moko.menuAction(2) == Game::Action::SHEAR);
    assert(moko.menuAction(4) == Game::Action::PROFILE);
}

static void test_baby_and_young_menu_have_no_shear_or_polish() {
    const Game::Stage stages[] = { Game::Stage::BABY, Game::Stage::YOUNG_MOKO,
                                   Game::Stage::YOUNG_SUFFOLK, Game::Stage::YOUNG_WILD };
    for (Game::Stage s : stages) {
        Game g = make_stage_game(s);
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

static void test_wool_and_horn_grow_only_for_adults() {
    // 毛・角は、毛刈り・角研ぎができる成体だけ伸ばす（若羊で伸ばすと、進化した時点で溜まったままになる）
    const Game::Stage young[] = { Game::Stage::BABY, Game::Stage::YOUNG_MOKO,
                                  Game::Stage::YOUNG_SUFFOLK, Game::Stage::YOUNG_WILD };
    for (Game::Stage s : young) {
        Game g = make_stage_game(s);
        for (uint32_t i = 0; i < 10 * Game::TICKS_PER_HOUR; ++i) g.tick();
        assert(g.wool() == 0);
        assert(g.horn() == 0);
    }
    Game moko = make_adult_game(Game::Breed::CORRIEDALE);
    for (uint32_t i = 0; i < 10 * Game::TICKS_PER_HOUR; ++i) moko.tick();
    assert(moko.wool() == 20);                           // 成体は 1 時間に +2
    assert(moko.horn() == 0);
    Game wild = make_adult_game(Game::Breed::MOUFLON);
    for (uint32_t i = 0; i < 10 * Game::TICKS_PER_HOUR; ++i) wild.tick();
    assert(wild.horn() == 20);
    assert(wild.wool() == 0);
}

// 進化の直前（あと 1 tick で進化する）の Game を作る。毛・角は古いセーブで溜まっている想定にする。
static Game make_about_to_evolve(Game::Stage stage, uint32_t days) {
    Game base(nullptr);
    skip_naming(base);
    GameSaveData d = base.saveData();
    d.stage     = uint8_t(stage);
    d.wool      = 70;
    d.horn      = 70;
    d.age_ticks = days * Game::TICKS_PER_DAY - 1;
    return Game(nullptr, &d);
}

static void test_evolution_resets_menu_cursor_when_item_count_changes() {
    // 若羊 → 成体で、項目数が 5 → 6 に変わるので、開いたまま進化してもカーソルの意味がずれないよう先頭へ戻す
    Game g = make_about_to_evolve(Game::Stage::YOUNG_MOKO, 7);
    g.onButton(Game::Button::CENTER);                    // メニューを開く
    for (int i = 0; i < 4; ++i) g.onButton(Game::Button::RIGHT);
    assert(g.menuCursor() == 4);                         // 若羊のメニューの末尾（もどる）
    g.tick();                                            // 7 日めに入り、成体へ進化
    assert(g.stage() == Game::Stage::ADULT);
    assert(g.menuCount() == 6);
    assert(g.menuCursor() == 0);
}

static void test_evolution_clears_wool_and_horn() {
    Game young = make_about_to_evolve(Game::Stage::BABY, 3);
    young.tick();                                        // ベビー → 若羊
    assert(young.stage() != Game::Stage::BABY);
    assert(young.wool() == 0 && young.horn() == 0);
    Game adult = make_about_to_evolve(Game::Stage::YOUNG_WILD, 7);
    adult.tick();                                        // 若羊 → 成体
    assert(adult.stage() == Game::Stage::ADULT);
    assert(adult.wool() == 0 && adult.horn() == 0);
}

static void test_menu_label_polish_for_wild() {
    Game g = make_adult_game(Game::Breed::MOUFLON);
    assert(g.family() == Game::Family::WILD);
    assert(std::string(g.menuLabel(2)) == "角研ぎ");   // POLISH（ワイルド系は毛ではなく角）
    assert(g.menuAction(2) == Game::Action::POLISH);
}

static void test_menu_labels_renderable_and_bounded() {
    // 全字がフォントに収録されていて（未収録は空白表示になる）、2 倍表示（16px/字）で
    // 左右の < > と重ならない幅に収まる。ワイルド系のラベルも含めて確認する。
    Game wild = make_adult_game(Game::Breed::MOUFLON);
    Game baby(nullptr);
    skip_naming(baby);
    Game moko = make_adult_game(Game::Breed::CORRIEDALE);

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
    mini_now = 100000; g.setNowMs(mini_now);
    menu_select(g, Game::Action::MINI);
    mini_play(g, 3);
    assert(g.jump().state() == JumpGame::State::OVER);
    assert(g.jump().score() == 3);
    assert(g.happy() == 50 + 3 * 2);
    assert(g.hunger() == 50 - 5);
    assert(g.miniReward() == 6);
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
static Game make_adult_with_growth(Game::Breed breed) {
    Game base(nullptr);
    skip_naming(base);
    GameSaveData d = base.saveData();
    d.stage = uint8_t(Game::Stage::ADULT);
    d.breed = uint8_t(breed);
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
    assert(g.pendingSfx() == Game::Sfx::NONE);         // 撫でるは、その場では鳴らさない（音は動きの進行に合わせる）

    Game moko = make_adult_with_growth(Game::Breed::CORRIEDALE);
    menu_select(moko, Game::Action::SHEAR);
    assert(moko.pendingSfx() == Game::Sfx::JOKI);

    Game wild = make_adult_with_growth(Game::Breed::MOUFLON);
    menu_select(wild, Game::Action::POLISH);
    assert(wild.pendingSfx() == Game::Sfx::JOKI);
}

static void test_no_sound_when_action_has_no_effect() {
    // 毛が短くて刈れないとき・就寝中の撫でる（羊は起きない）は、音を予約しない
    Game g = make_adult_game(Game::Breed::CORRIEDALE);   // 毛は 0
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
    // ご飯はぱくっを 1 秒おきに 3 回するので、他の動きより長い（FEED_ACTION_TICKS）。
    // 撫でる・毛刈りなどは ACTION_TICKS。
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

    Game base(nullptr);
    skip_naming(base);
    GameSaveData d = base.saveData();
    d.stage = uint8_t(Game::Stage::ADULT);
    d.breed = uint8_t(Game::Breed::LINCOLN);
    d.wool  = 50;
    Game shear(nullptr, &d);
    menu_select(shear, Game::Action::SHEAR);
    for (int i = 0; i < Game::TRIM_ACTION_TICKS; ++i) shear.tick();
    assert(shear.action() == Game::Action::SHEAR);
    shear.tick();
    assert(shear.action() == Game::Action::NONE);

    assert(Game::FEED_ACTION_TICKS > Game::ACTION_TICKS);
    assert(Game::TRIM_ACTION_TICKS > Game::ACTION_TICKS);   // 毛刈り・角研ぎは、音のあとに動きが始まるので長い
}

// ---- 撫でる：ハートと弾み -------------------------------------------------------
static void test_pet_stroke_and_bleat_timing() {
    using namespace pet_motion;
    // 撫でるのは 1 回。撫でたあと、ポッと鳴き声が別の音に聞こえるよう、少し間をおいて鳴き声が鳴る。
    assert(isStroke(STROKE_TICK));
    assert(!isStroke(STROKE_TICK - 1) && !isStroke(STROKE_TICK + 1) && !isStroke(0));
    assert(BLEAT_TICK - STROKE_TICK >= 5);                     // 0.25 秒以上あける
    // 鳴き声は、鳴り終わる（BLEAT_BLOCK_TICKS 止まる）までに、動きが終わらない
    assert(BLEAT_TICK + BLEAT_BLOCK_TICKS <= Game::ACTION_TICKS);
}

static void test_pet_bounce_only_at_the_stroke() {
    using namespace pet_motion;
    assert(bounce(0) == 0);
    for (int t = STROKE_TICK; t < STROKE_TICK + BOUNCE_LEN; ++t) assert(bounce(t) == -BOUNCE_PX);
    assert(bounce(STROKE_TICK - 1) == 0);
    assert(bounce(STROKE_TICK + BOUNCE_LEN) == 0);
}

static void test_pet_heart_rises_and_leaves_the_screen() {
    using namespace pet_motion;
    // 撫でる前にはない。撫でたら出て、時間とともに上がる。
    assert(!heartShown(STROKE_TICK - 1));
    assert(heartShown(STROKE_TICK));
    assert(heartY(STROKE_TICK + 8) < heartY(STROKE_TICK));
    // 鳴き声で画面が止まる瞬間（BLEAT_TICK）は、ハートが画面の中に見えている（止まった絵にハートがある）
    assert(heartY(BLEAT_TICK) > -HEART_H && heartY(BLEAT_TICK) < 64);
    // 動きが終わるときには、画面の上へ出切っている（ふっと消えるのではなく、浮いて去る）
    assert(heartY(Game::ACTION_TICKS) + HEART_H <= 0);
    // 出るときは、羊の頭（2 倍スプライトの内容は y=20 から、弾みで -1）より上
    assert(heartY(STROKE_TICK) + HEART_H - 1 < 19);
}

static void test_pet_heart_is_centered_over_the_sheep_and_on_screen() {
    using namespace pet_motion;
    // 羊が歩く範囲（0〜80）のどこでも、ハートは羊の頭の真上（中央揃え）で、画面に収まる
    for (int walk_x = 0; walk_x <= 80; ++walk_x) {
        const int left = heartX(walk_x);
        assert(left >= 0 && left + HEART_W <= background::W);
        const int center2 = 2 * left + HEART_W;                  // ハートの中心の 2 倍
        const int sheep2  = 2 * walk_x + 48;                     // 羊（48px 幅）の中心の 2 倍
        assert(center2 - sheep2 >= -2 && center2 - sheep2 <= 2);
    }
}

static void test_pet_heart_shape() {
    using namespace pet_motion;
    // 前より大きい（5x5 → 7x7）。左右対称で、下が尖っている。
    assert(HEART_W == 7 && HEART_H == 7);
    int n = 0;
    for (int dy = 0; dy < HEART_H; ++dy) {
        for (int dx = 0; dx < HEART_W; ++dx) {
            if (heartPixel(dx, dy)) ++n;
            assert(heartPixel(dx, dy) == heartPixel(HEART_W - 1 - dx, dy));   // 左右対称
        }
    }
    assert(n == 34);
    assert(heartPixel(HEART_W / 2, HEART_H - 1));                // 下の先端
    assert(!heartPixel(0, HEART_H - 1) && !heartPixel(HEART_W - 1, HEART_H - 1));
    assert(!heartPixel(HEART_W / 2, 0));                          // 上は真ん中がくぼむ
    assert(!heartPixel(-1, 0) && !heartPixel(HEART_W, 0) && !heartPixel(0, HEART_H));
}

extern int g_stub_blip_count;    // test/stubs/stub_sound.cpp（Sound::blip が呼ばれた回数）

static void test_pet_blips_at_the_stroke_and_bleats_after() {
    Sound sound(0);
    Game base(nullptr);
    skip_naming(base);
    GameSaveData d = base.saveData();
    Game g(&sound, &d);
    menu_select(g, Game::Action::PET);
    g_stub_blip_count = 0;
    int blips = 0, blip_tick = -1;
    bool bleat_seen = false;
    for (int t = 1; t <= Game::ACTION_TICKS; ++t) {
        const int before = g_stub_blip_count;
        g.setNowMs(uint32_t(t) * 50u);
        g.tick();
        if (g_stub_blip_count > before) { ++blips; blip_tick = t; }
        if (t < pet_motion::BLEAT_TICK) assert(g.pendingSfx() == Game::Sfx::NONE);
        if (t == pet_motion::BLEAT_TICK) { assert(g.pendingSfx() == Game::Sfx::MEE); bleat_seen = true; }
    }
    assert(blips == 1);                                   // 撫でたときに 1 回だけ、短い音（待たない音）
    assert(blip_tick == pet_motion::STROKE_TICK);
    assert(bleat_seen);                                   // そのあとに鳴き声（待つ音）を予約する
    g.playPendingSfx();
    assert(g.pendingSfx() == Game::Sfx::NONE);
}

// ---- 進化の重み（空腹・幸福の値だけで決め、残りは乱数）--------------------------
// 重みは、乱数と切り離した純粋関数にしてあるので、テストで直接確かめる。
static void test_young_choice_weights() {
    Game::EvoChoice c[3];
    Game::youngChoices(100, 100, c);             // 満腹で幸福
    assert(c[0].stage == Game::Stage::YOUNG_MOKO    && c[0].weight == 90);   // 50 + 100 * 40%
    assert(c[1].stage == Game::Stage::YOUNG_SUFFOLK && c[1].weight == 90);   // 50 + 100 * 40%
    assert(c[2].stage == Game::Stage::YOUNG_WILD    && c[2].weight == 50);   // 50 + (200 - 200) * 20%
    Game::youngChoices(0, 0, c);                 // 空腹で不幸: ワイルド系が一番出やすい
    assert(c[0].weight == 50 && c[1].weight == 50 && c[2].weight == 90);
    Game::youngChoices(100, 0, c);               // 満腹だけ: モコ系
    assert(c[0].weight == 90 && c[1].weight == 50 && c[2].weight == 70);
    Game::youngChoices(0, 100, c);               // 幸福だけ: サフォーク系
    assert(c[0].weight == 50 && c[1].weight == 90 && c[2].weight == 70);
    for (int i = 0; i < 3; ++i) assert(c[i].breed == Game::Breed::NONE);
}

static void test_choice_weights_stay_in_range_for_out_of_range_values() {
    // 範囲外の値（負・100 超）でも、重みが基本値（50）より小さくならず、上限を超えない
    Game::EvoChoice c[3];
    Game::youngChoices(-30, 250, c);
    for (int i = 0; i < 3; ++i) assert(c[i].weight >= 50 && c[i].weight <= 90);
    Game::EvoChoice a[2];
    assert(Game::adultChoices(Game::Stage::YOUNG_SUFFOLK, 300, -10, a) == 2);
    for (int i = 0; i < 2; ++i) assert(a[i].weight >= 50 && a[i].weight <= 100);
}

static void test_adult_choice_weights() {
    Game::EvoChoice c[2];
    // モコ系: CORRIEDALE / LINCOLN を半々（MERINO は抽選に入らない）
    assert(Game::adultChoices(Game::Stage::YOUNG_MOKO, 100, 100, c) == 2);
    assert(c[0].breed == Game::Breed::CORRIEDALE && c[0].weight == 50);
    assert(c[1].breed == Game::Breed::LINCOLN    && c[1].weight == 50);
    for (int i = 0; i < 2; ++i) assert(c[i].stage == Game::Stage::ADULT);
    // サフォーク系: 幸福なほど HAMPSHIRE が出やすい
    assert(Game::adultChoices(Game::Stage::YOUNG_SUFFOLK, 100, 100, c) == 2);
    assert(c[0].breed == Game::Breed::SUFFOLK   && c[0].weight == 50);
    assert(c[1].breed == Game::Breed::HAMPSHIRE && c[1].weight == 100);      // 50 + 100 * 50%
    Game::adultChoices(Game::Stage::YOUNG_SUFFOLK, 100, 0, c);
    assert(c[1].weight == 50);
    // ワイルド系: 満腹なほど BIGHORN が出やすい
    assert(Game::adultChoices(Game::Stage::YOUNG_WILD, 100, 100, c) == 2);
    assert(c[0].breed == Game::Breed::MOUFLON && c[0].weight == 50);
    assert(c[1].breed == Game::Breed::BIGHORN && c[1].weight == 100);
    Game::adultChoices(Game::Stage::YOUNG_WILD, 0, 100, c);
    assert(c[1].weight == 50);
    // 若羊でなければ、選択肢はない
    assert(Game::adultChoices(Game::Stage::BABY, 50, 50, c) == 0);
    assert(Game::adultChoices(Game::Stage::ADULT, 50, 50, c) == 0);
}

static void test_pick_weighted_uses_the_whole_roll() {
    Game::EvoChoice c[3] = {
        { Game::Stage::YOUNG_MOKO,    Game::Breed::NONE, 30 },
        { Game::Stage::YOUNG_SUFFOLK, Game::Breed::NONE, 20 },
        { Game::Stage::YOUNG_WILD,    Game::Breed::NONE, 50 },
    };
    assert(Game::pickWeighted(c, 3, 0)   == 0);
    assert(Game::pickWeighted(c, 3, 29)  == 0);
    assert(Game::pickWeighted(c, 3, 30)  == 1);
    assert(Game::pickWeighted(c, 3, 49)  == 1);
    assert(Game::pickWeighted(c, 3, 50)  == 2);
    assert(Game::pickWeighted(c, 3, 99)  == 2);
    assert(Game::pickWeighted(c, 3, 100) == 0);                     // roll は、合計で割った余りで使う
    assert(Game::pickWeighted(c, 3, 4000000099u) == 2);             // 32 ビットの乱数の全範囲を使う
    Game::EvoChoice zero[2] = {
        { Game::Stage::ADULT, Game::Breed::MOUFLON, 0 },
        { Game::Stage::ADULT, Game::Breed::BIGHORN, 0 },
    };
    assert(Game::pickWeighted(zero, 2, 12345) == 0);                // 合計が 0 なら 0 番目
}

// ---- 進化の結果 ----------------------------------------------------------------
static Game make_evolving(Game::Stage stage, uint32_t days, int hunger, int happy) {
    Game base(nullptr);
    skip_naming(base);
    GameSaveData d = base.saveData();
    d.stage     = uint8_t(stage);
    d.hunger    = uint8_t(hunger);
    d.happy     = uint8_t(happy);
    d.age_ticks = days * Game::TICKS_PER_DAY - 1;
    return Game(nullptr, &d);
}

static void test_adult_evolution_never_picks_merino() {
    int corriedale = 0, lincoln = 0;
    for (int i = 0; i < 300; ++i) {
        Game g = make_evolving(Game::Stage::YOUNG_MOKO, 7, 100, 100);
        g.tick();
        assert(g.stage() == Game::Stage::ADULT);
        assert(g.breed() != Game::Breed::MERINO);
        if (g.breed() == Game::Breed::CORRIEDALE) ++corriedale;
        if (g.breed() == Game::Breed::LINCOLN)    ++lincoln;
    }
    assert(corriedale > 0 && lincoln > 0);                // 2 品種とも出る（乱数で決まる）
    assert(corriedale + lincoln == 300);
}

static void test_young_evolution_follows_hunger_and_happy() {
    // 空腹・不幸なほどワイルド系が出やすく、満腹・幸福なほど出にくい（重み 88 対 50 なので、300 回なら十分に差が出る）
    int wild_low = 0, wild_high = 0;
    for (int i = 0; i < 300; ++i) {
        Game low = make_evolving(Game::Stage::BABY, 3, 5, 5);
        low.tick();
        if (low.stage() == Game::Stage::YOUNG_WILD) ++wild_low;
        Game high = make_evolving(Game::Stage::BABY, 3, 100, 100);
        high.tick();
        if (high.stage() == Game::Stage::YOUNG_WILD) ++wild_high;
    }
    assert(wild_low > wild_high + 30);
}

static void test_young_evolution_can_pick_every_family() {
    // 抽選が乱数の全範囲を使うこと（0〜127 だけだと、最後の選択肢が選ばれにくい）
    int count[3] = {0, 0, 0};
    for (int i = 0; i < 600; ++i) {
        Game g = make_evolving(Game::Stage::BABY, 3, 100, 100);
        g.tick();
        if (g.stage() == Game::Stage::YOUNG_MOKO)    ++count[0];
        if (g.stage() == Game::Stage::YOUNG_SUFFOLK) ++count[1];
        if (g.stage() == Game::Stage::YOUNG_WILD)    ++count[2];
    }
    assert(count[0] + count[1] + count[2] == 600);
    assert(count[2] > 60);                                 // ワイルド系（重み 50 / 合計 230）も、ちゃんと出る
}

// ---- セーブ形式は変えない -------------------------------------------------------
static void test_old_save_with_tend_values_still_loads() {
    Game base(nullptr);
    skip_naming(base);
    GameSaveData d = base.saveData();
    d.tend_feed = 20; d.tend_pet = 30; d.tend_shear = 40; d.tend_polish = 50;   // 古いセーブの値
    Game g(nullptr, &d);
    assert(g.stage() == base.stage());                    // 読み込める
    GameSaveData out = g.saveData();
    assert(out.tend_feed == 0 && out.tend_pet == 0 && out.tend_shear == 0 && out.tend_polish == 0);
    assert(out.magic == 0x4D4F4B35u);                     // MAGIC は変えない（今の羊のセーブを消さない）
}

// ---- エンディング曲（メロディの進行）-------------------------------------------
// 音符列を、時刻から進める純粋なロジック（ブザーには触らない）。ミニゲームと同じく待たずに進めるので、
// ループの遅れで何音分もまとめて進んでも、鳴らす音は今の時刻に合ったものになる。
static const melody::Note kTestNotes[] = {
    { 440, 100 },   // 0〜100 ms（最後の 30 ms は、音の切れ目として無音）
    {   0,  50 },   // 100〜150 ms 休符
    { 880, 100 },   // 150〜250 ms
};
static const int kTestNoteCount = 3;

static void test_melody_player_follows_time() {
    melody::MelodyPlayer m;
    assert(!m.playing());
    m.start(kTestNotes, kTestNoteCount, 1000);
    assert(m.playing());
    assert(m.update(1000) && m.freq() == 440);        // 鳴り始め（変化あり）
    assert(!m.update(1060) && m.freq() == 440);       // 同じ音が続く（変化なし）
    assert(m.update(1075) && m.freq() == 0);          // 音の切れ目（最後の 30 ms）
    assert(!m.update(1120) && m.freq() == 0);         // 切れ目から休符へ続く（変化なし）
    assert(m.update(1150) && m.freq() == 880);
    assert(m.update(1225) && m.freq() == 0);          // 最後の音の切れ目
    assert(m.playing());
    assert(!m.update(1249) && m.freq() == 0);
    assert(m.update(1250) == false && !m.playing());  // 終わり。無音のまま変化なし
    assert(!m.update(2000));                          // 終わったあとは何も起きない
}

static void test_melody_player_catches_up_after_a_late_update() {
    melody::MelodyPlayer m;
    m.start(kTestNotes, kTestNoteCount, 0);
    assert(m.update(0) && m.freq() == 440);
    assert(m.update(170) && m.freq() == 880);         // 遅れて呼んでも、今の時刻の音になる
    melody::MelodyPlayer late;
    late.start(kTestNotes, kTestNoteCount, 0);
    late.update(5000);
    assert(!late.playing() && late.freq() == 0);       // 曲の長さを超えたら終わる
}

static void test_melody_player_starting_with_a_rest() {
    // 先頭が休符の曲は、無音で始まり、休符が終わると鳴り始める
    static const melody::Note rest_first[] = { { 0, 100 }, { 660, 100 } };
    melody::MelodyPlayer m;
    m.start(rest_first, 2, 10);
    assert(!m.update(10) && m.freq() == 0);
    assert(m.update(110) && m.freq() == 660);
}

static void test_melody_player_stop_and_restart() {
    melody::MelodyPlayer m;
    m.start(kTestNotes, kTestNoteCount, 0);
    m.update(0);
    m.stop();
    assert(!m.playing());
    assert(m.freq() == 0);
    assert(!m.update(20));
    m.start(kTestNotes, kTestNoteCount, 500);          // 止めたあと、最初から始め直せる
    assert(m.update(500) && m.freq() == 440);
}

static void test_melody_player_handles_millisecond_counter_wraparound() {
    melody::MelodyPlayer m;
    m.start(kTestNotes, kTestNoteCount, 0xFFFFFFF0u);
    m.update(0xFFFFFFF0u);
    assert(m.update(0xFFFFFFF0u + 160u) && m.freq() == 880);   // now_ms が 32 ビットを一周してもずれない
}

// 曲のデータ（実機で聞いて直しやすいよう、性質だけを確かめる）
static void test_ending_melody_shape() {
    assert(melody::kEndingCount > 0);
    uint32_t total = 0;
    int last_pitch = 0;
    for (int i = 0; i < melody::kEndingCount; ++i) {
        const melody::Note& n = melody::kEnding[i];
        assert(n.dur_ms >= 60 && n.dur_ms <= 4000);                        // 短すぎる・長すぎる音がない
        assert(n.freq_hz == 0 || (n.freq_hz >= 300 && n.freq_hz <= 2000)); // ブザーで出しやすい音域
        total += n.dur_ms;
        if (n.freq_hz != 0) last_pitch = n.freq_hz;
    }
    assert(total >= 28000 && total <= 32000);                              // 約 30 秒
    assert(last_pitch == 523);                                             // 主音（ド）で終わる
}

// ---- 亡くなったときの画面でエンディング曲を流す ---------------------------------
extern int g_stub_ending_started;   // test/stubs/stub_sound.cpp
extern int g_stub_melody_stopped;

// あと 1 tick で寿命を迎える Game（寿命 10 日、9 日と 1 tick 手前）
static Game make_about_to_die(Sound* sound) {
    Game base(nullptr);
    skip_naming(base);
    GameSaveData d = base.saveData();
    d.lifespan_days = 10;
    d.age_ticks     = 10 * Game::TICKS_PER_DAY - 1;
    return Game(sound, &d);
}

static void test_death_queues_the_ending_song_and_plays_after_drawing() {
    Sound sound(0);
    Game g = make_about_to_die(&sound);
    g.tick();
    assert(g.screen() == Game::Screen::GRAVE);
    assert(g.pendingSfx() == Game::Sfx::ENDING);        // 予約するだけ（画面を切り替えたあとに鳴らす）
    int before = g_stub_ending_started;
    g.playPendingSfx();
    assert(g_stub_ending_started == before + 1);
    assert(g.pendingSfx() == Game::Sfx::NONE);
}

static void test_starvation_also_queues_the_ending_song() {
    // 寿命でも餓死でも、同じ曲
    Game base(nullptr);
    skip_naming(base);
    GameSaveData d = base.saveData();
    d.hunger = 0;
    Game g(nullptr, &d);
    g.tick();
    assert(g.screen() == Game::Screen::GRAVE);
    assert(g.pendingSfx() == Game::Sfx::ENDING);
}

static void test_button_on_grave_screen_stops_the_song_and_starts_naming() {
    Sound sound(0);
    Game g = make_about_to_die(&sound);
    g.tick();
    g.playPendingSfx();
    int before = g_stub_melody_stopped;
    g.onButton(Game::Button::CENTER);
    assert(g_stub_melody_stopped == before + 1);        // 曲を止める
    assert(g.screen() == Game::Screen::NAMING);          // 次の命名へ進む
}

static void test_ending_song_without_sound_does_not_crash() {
    Game g = make_about_to_die(nullptr);
    g.tick();
    g.playPendingSfx();
    g.onButton(Game::Button::CENTER);
    assert(g.screen() == Game::Screen::NAMING);
}

// ---- 検証用：LEFT 長押しで、進化の順番に 1 つずつ切り替える（DEBUG_FAST / HOST_TEST のみ） ----
static void cycle_form(Game& g) { g.onButton(Game::Button::LEFT_LONG); }

static void test_debug_cycle_walks_every_form_in_order() {
    Game g(nullptr);
    skip_naming(g);
    assert(g.stage() == Game::Stage::BABY);
    struct Form { Game::Stage stage; Game::Breed breed; };
    const Form order[] = {
        { Game::Stage::YOUNG_MOKO,    Game::Breed::NONE },
        { Game::Stage::ADULT,         Game::Breed::CORRIEDALE },
        { Game::Stage::ADULT,         Game::Breed::LINCOLN },
        { Game::Stage::YOUNG_SUFFOLK, Game::Breed::NONE },
        { Game::Stage::ADULT,         Game::Breed::SUFFOLK },
        { Game::Stage::ADULT,         Game::Breed::HAMPSHIRE },
        { Game::Stage::YOUNG_WILD,    Game::Breed::NONE },
        { Game::Stage::ADULT,         Game::Breed::MOUFLON },
        { Game::Stage::ADULT,         Game::Breed::BIGHORN },
        { Game::Stage::BABY,          Game::Breed::NONE },      // 1 周して、ベビーへ戻る
        { Game::Stage::YOUNG_MOKO,    Game::Breed::NONE },
    };
    for (const Form& f : order) {
        cycle_form(g);
        assert(g.stage() == f.stage);
        assert(g.breed() == f.breed);
    }
}

static void test_debug_cycle_keeps_the_form_from_evolving_on_its_own() {
    // 切り替えた姿は、その段階の年齢にそろえる。年齢がずれていると、次の tick で勝手に進化してしまう
    Game g(nullptr);
    skip_naming(g);
    for (int i = 0; i < 10; ++i) {
        cycle_form(g);
        const Game::Stage stage = g.stage();
        const Game::Breed breed = g.breed();
        for (int t = 0; t < 100; ++t) g.tick();
        assert(g.stage() == stage);
        assert(g.breed() == breed);
    }
}

static void test_debug_cycle_keeps_the_sheep_alive_and_fed() {
    // 姿を確かめている間に、寿命や空腹で死なないようにする（寿命は最大、空腹・幸福は満タン）
    Game g(nullptr);
    skip_naming(g);
    for (int i = 0; i < 3; ++i) cycle_form(g);            // リンカーン（成体、7 日め）
    const GameSaveData d = g.saveData();
    assert(d.lifespan_days == 255);
    assert(d.hunger == 100 && d.happy == 100);
    for (uint32_t t = 0; t < 3 * Game::TICKS_PER_DAY; ++t) g.tick();
    assert(g.screen() != Game::Screen::GRAVE);
    assert(g.breed() == Game::Breed::LINCOLN);
}

static void test_debug_cycle_resets_menu_cursor_and_wool() {
    Game g(nullptr);
    skip_naming(g);
    cycle_form(g);                                         // 若羊（メニュー 5 項目）
    cycle_form(g);                                         // 成体（メニュー 6 項目）
    assert(g.menuCount() == 6);
    g.onButton(Game::Button::CENTER);                      // メニューを開く
    for (int i = 0; i < 5; ++i) g.onButton(Game::Button::RIGHT);
    assert(g.menuCursor() == 5);
    g.onButton(Game::Button::LEFT_LONG);                   // メニューの中では切り替えない
    assert(g.breed() == Game::Breed::CORRIEDALE);
    g.onButton(Game::Button::CENTER);                      // 「もどる」
    cycle_form(g);
    assert(g.breed() == Game::Breed::LINCOLN);
    assert(g.wool() == 0);
    assert(g.menuCursor() == 0);
}

static void test_debug_cycle_treats_old_merino_as_corriedale() {
    Game base(nullptr);
    skip_naming(base);
    GameSaveData d = base.saveData();
    d.stage = uint8_t(Game::Stage::ADULT);
    d.breed = uint8_t(Game::Breed::MERINO);
    d.age_ticks = 7 * Game::TICKS_PER_DAY;
    Game g(nullptr, &d);
    cycle_form(g);
    assert(g.breed() == Game::Breed::LINCOLN);             // コリデールの次
}

// ---- 毛刈り：はさみが体を 2 往復して、毛の束が落ちる ------------------------------
static void test_shear_scissors_sweep_the_body_twice() {
    using namespace shear_motion;
    // 音（画面が止まる間）が終わってから出て、2 回目が終わる（CUT_TICK）まで見える
    assert(PASS_START[0] >= SOUND_BLOCK_TICKS);
    assert(!clipperShown(PASS_START[0] - 1));
    assert(clipperShown(PASS_START[0]));
    assert(clipperShown(CUT_TICK - 1));
    assert(!clipperShown(CUT_TICK));
    assert(PASS_START[1] == PASS_START[0] + PASS_LEN);        // 1 回目の直後に 2 回目
    // 1 回目は右へ、2 回目は左へ動く。2 回目は 1 回目より下
    for (int walk_x : { 0, 40, 80 }) {
        assert(clipperX(walk_x, PASS_START[0]) == walk_x + PASS_X0);
        assert(clipperX(walk_x, PASS_START[0] + PASS_LEN - 1) == walk_x + PASS_X1);
        assert(clipperX(walk_x, PASS_START[1]) == walk_x + PASS_X1);
        assert(clipperX(walk_x, PASS_START[1] + PASS_LEN - 1) == walk_x + PASS_X0);
        for (int t = PASS_START[0]; t < CUT_TICK; ++t) {
            // 動いている間は、羊の 2 倍スプライト（48px 幅）の中、画面の中
            assert(clipperX(walk_x, t) >= walk_x && clipperX(walk_x, t) + SCISSORS_W <= walk_x + 48);
            assert(clipperX(walk_x, t) >= 0 && clipperX(walk_x, t) + SCISSORS_W <= background::W);
        }
    }
    assert(clipperY(PASS_START[0]) == PASS_Y[0]);
    assert(clipperY(PASS_START[1]) == PASS_Y[1]);
    assert(PASS_Y[1] > PASS_Y[0]);
    // 刃は開閉を繰り返す
    bool saw_open = false, saw_closed = false;
    for (int t = PASS_START[0]; t < CUT_TICK; ++t) (clipperOpen(t) ? saw_open : saw_closed) = true;
    assert(saw_open && saw_closed);
}

static void test_shear_wool_is_cut_after_the_second_pass() {
    using namespace shear_motion;
    assert(!woolCut(0));
    assert(!woolCut(CUT_TICK - 1));                            // 2 回目の最後まで、毛は付いている
    assert(woolCut(CUT_TICK));
    assert(woolCut(Game::TRIM_ACTION_TICKS));
    // 毛が消えたあと、動きが終わるまでに、毛の束が積もったのを見せる時間がある
    assert(Game::TRIM_ACTION_TICKS - CUT_TICK >= 10);
}

static void test_shear_tufts_fall_and_pile_up_on_the_ground() {
    using namespace shear_motion;
    int x, y;
    for (int i = 0; i < TUFT_COUNT; ++i) {
        // 落ち始める前は出ない。はさみが出ている間に落ち始める
        assert(TUFT_SPAWN[i] >= PASS_START[0] && TUFT_SPAWN[i] < CUT_TICK);
        assert(!tuft(i, 40, TUFT_SPAWN[i] - 1, &x, &y));
        assert(tuft(i, 40, TUFT_SPAWN[i], &x, &y));
        int prev_y = y;
        for (int t = TUFT_SPAWN[i]; t <= Game::TRIM_ACTION_TICKS; ++t) {
            assert(tuft(i, 40, t, &x, &y));
            assert(y >= prev_y);                               // 下へ落ちる（戻らない）
            assert(y + TUFT_H - 1 <= GROUND_Y);                // 地面（草の帯）より下へは行かない
            assert(y >= 0);
            assert(x >= 40 && x + TUFT_W <= 40 + 48);          // 羊の幅の中
            prev_y = y;
        }
        assert(y + TUFT_H - 1 == GROUND_Y);                    // 最後は地面に積もる
    }
    assert(GROUND_Y + 1 == background::GRASS_TOP);
    // 落ち始めは、はさみのいる場所の近く
    assert(tuft(0, 0, TUFT_SPAWN[0], &x, &y));
    assert(x >= clipperX(0, TUFT_SPAWN[0]) - TUFT_W && x <= clipperX(0, TUFT_SPAWN[0]) + SCISSORS_W);
}

static int shape_count(bool (*f)(int, int), int w, int h) {
    int n = 0;
    for (int dy = 0; dy < h; ++dy) for (int dx = 0; dx < w; ++dx) if (f(dx, dy)) ++n;
    return n;
}

static void test_shear_shapes_stay_inside_their_boxes() {
    using namespace shear_motion;
    assert(!scissorsPixel(true, -1, 0) && !scissorsPixel(true, SCISSORS_W, 0));
    assert(!scissorsPixel(true, 0, -1) && !scissorsPixel(true, 0, SCISSORS_H));
    assert(!tuftPixel(-1, 0) && !tuftPixel(TUFT_W, 0) && !tuftPixel(0, -1) && !tuftPixel(0, TUFT_H));
    assert(shape_count(tuftPixel, TUFT_W, TUFT_H) > 0);
    int open = 0, closed = 0;
    for (int dy = 0; dy < SCISSORS_H; ++dy)
        for (int dx = 0; dx < SCISSORS_W; ++dx) {
            open += scissorsPixel(true, dx, dy);
            closed += scissorsPixel(false, dx, dy);
        }
    assert(open > 0 && closed > 0);
    // 開いた絵と閉じた絵は違う（開閉が見える）
    bool differs = false;
    for (int dy = 0; dy < SCISSORS_H; ++dy)
        for (int dx = 0; dx < SCISSORS_W; ++dx)
            if (scissorsPixel(true, dx, dy) != scissorsPixel(false, dx, dy)) differs = true;
    assert(differs);
}

// ---- 角研ぎ：木の幹に角をこすりつけて、火花が散る ---------------------------------
static void test_polish_rubs_between_the_sound_and_the_filed_tick() {
    using namespace polish_motion;
    assert(RUB_START >= SOUND_BLOCK_TICKS);                    // 音が終わってから、こすり始める
    assert(!isRubbing(RUB_START - 1) && isRubbing(RUB_START));
    assert(isRubbing(RUB_END - 1) && !isRubbing(RUB_END));
    assert(!filed(FILED_TICK - 1) && filed(FILED_TICK));       // こすり終わったら、角が短くなる
    assert(Game::TRIM_ACTION_TICKS - FILED_TICK >= 10);        // 研ぎ終えた羊を見せる時間がある
}

static void test_polish_leans_toward_the_trunk_and_back() {
    using namespace polish_motion;
    int dx, dy;
    lean(0, 1, &dx, &dy);
    assert(dx == 0 && dy == 0);                                // こする前は、傾かない
    lean(RUB_END, 1, &dx, &dy);
    assert(dx == 0 && dy == 0);                                // こすり終えたら、戻る
    bool pressed = false, released = false;
    for (int t = RUB_START; t < RUB_END; ++t) {
        lean(t, 1, &dx, &dy);
        assert(dx == (isPressed(t) ? LEAN_PX : LEAN_PX - 2));  // 幹へ押しつける・戻すを繰り返す
        assert(dy == 0);
        (isPressed(t) ? pressed : released) = true;
        lean(t, -1, &dx, &dy);
        assert(dx == -(isPressed(t) ? LEAN_PX : LEAN_PX - 2)); // 左に幹があるときは、逆向き
    }
    assert(pressed && released);
    assert(!isPressed(RUB_START - 1) && !isPressed(RUB_END));
}

static void test_polish_sparks_only_while_pressed() {
    using namespace polish_motion;
    for (int t = 0; t <= Game::TRIM_ACTION_TICKS; ++t) assert(sparkShown(t) == isPressed(t));
    // 火花は、幹と羊のすき間（角の先）、角の高さに出る
    for (int walk_x : { 0, 30, 56, 57, 80 }) {
        int x, y;
        sparkPos(walk_x, &x, &y);
        assert(x >= 0 && x + SPARK_W <= background::W);
        assert(y + SPARK_H / 2 == SPARK_CENTER_Y);
        if (direction(walk_x) > 0) assert(x + SPARK_W - 1 < trunkLeft(walk_x) + 1);   // 幹の手前（羊側）
        else                        assert(x >= trunkLeft(walk_x) + TRUNK_W - 1);
    }
}

static void test_polish_trunk_stays_on_screen_and_beside_the_sheep() {
    using namespace polish_motion;
    assert(direction(0) == 1 && direction(56) == 1);
    assert(direction(57) == -1 && direction(80) == -1);        // 画面の右半分では、左に置く
    for (int walk_x = 0; walk_x <= 80; ++walk_x) {
        const int left = trunkLeft(walk_x);
        assert(left >= 0 && left + TRUNK_W - 1 < background::W);
        if (direction(walk_x) > 0) assert(left >= walk_x + 44);            // 右に置くときは、羊の右端の外
        else                        assert(left + TRUNK_W - 1 <= walk_x + 4);
    }
    assert(TRUNK_TOP + TRUNK_H - 1 + 1 == background::GRASS_TOP);          // 根元は草の帯の真上
    assert(TRUNK_TOP <= SPARK_CENTER_Y - SPARK_H / 2);                     // 火花の高さに、幹がある
}

static void test_polish_shapes_stay_inside_their_boxes() {
    using namespace polish_motion;
    assert(!trunkPixel(-1, 0) && !trunkPixel(TRUNK_W, 0) && !trunkPixel(0, -1) && !trunkPixel(0, TRUNK_H));
    assert(!sparkPixel(-1, 0) && !sparkPixel(SPARK_W, 0) && !sparkPixel(0, -1) && !sparkPixel(0, SPARK_H));
    for (int dy = 0; dy < TRUNK_H; ++dy) {
        assert(trunkPixel(0, dy) && trunkPixel(TRUNK_W - 1, dy));          // 幹の両側の輪郭は、途切れない
    }
    for (int dx = 0; dx < TRUNK_W; ++dx) assert(trunkPixel(dx, 0));         // 上端は閉じている
    assert(shape_count(sparkPixel, SPARK_W, SPARK_H) > 0);
    assert(sparkPixel(SPARK_W / 2, SPARK_H / 2));                          // 中心は点く
}

// ---- 毛刈り・角研ぎの絵の切り替え：始めたときが、ふさふさ・角長だったか ----------
static void test_trim_remembers_whether_the_sheep_was_grown() {
    for (Game::Breed breed : { Game::Breed::CORRIEDALE, Game::Breed::MOUFLON }) {
        const Game::Action act = breed == Game::Breed::MOUFLON ? Game::Action::POLISH : Game::Action::SHEAR;
        Game base(nullptr);
        skip_naming(base);
        GameSaveData d = base.saveData();
        d.stage = uint8_t(Game::Stage::ADULT);
        d.breed = uint8_t(breed);
        d.wool = d.horn = 80;                                  // ふさふさ・角長（> 50）
        Game grown(nullptr, &d);
        menu_select(grown, act);
        assert(grown.action() == act);
        assert(grown.actionWasGrown());                        // 毛・角は 0 に戻っているが、始めたときは伸びていた
        assert(!grown.isFluffy() && !grown.isLonghorn());

        d.wool = d.horn = 30;                                  // 刈れる（> 10）が、絵はまだ通常（<= 50）
        Game shortish(nullptr, &d);
        menu_select(shortish, act);
        assert(shortish.action() == act);
        assert(!shortish.actionWasGrown());                    // 絵の切り替えなし
    }
}

static void test_trim_actions_last_trim_action_ticks() {
    Game base(nullptr);
    skip_naming(base);
    GameSaveData d = base.saveData();
    d.stage = uint8_t(Game::Stage::ADULT);
    d.breed = uint8_t(Game::Breed::BIGHORN);
    d.horn = 80;
    Game g(nullptr, &d);
    menu_select(g, Game::Action::POLISH);
    for (int i = 0; i < Game::TRIM_ACTION_TICKS; ++i) g.tick();
    assert(g.action() == Game::Action::POLISH);
    g.tick();
    assert(g.action() == Game::Action::NONE);
}

int main() {
    std::setbuf(stdout, nullptr);
    std::srand(42);   // 進化判定の再現性のため固定シード
    std::fprintf(stderr, "\n=== game.cpp host tests (HOST_TEST=on) ===\n\n");

    RUN(test_default_state);
    RUN(test_debug_cycle_walks_every_form_in_order);
    RUN(test_debug_cycle_keeps_the_form_from_evolving_on_its_own);
    RUN(test_debug_cycle_keeps_the_sheep_alive_and_fed);
    RUN(test_debug_cycle_resets_menu_cursor_and_wool);
    RUN(test_debug_cycle_treats_old_merino_as_corriedale);
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
    RUN(test_shear_scissors_sweep_the_body_twice);
    RUN(test_shear_wool_is_cut_after_the_second_pass);
    RUN(test_shear_tufts_fall_and_pile_up_on_the_ground);
    RUN(test_shear_shapes_stay_inside_their_boxes);
    RUN(test_polish_rubs_between_the_sound_and_the_filed_tick);
    RUN(test_polish_leans_toward_the_trunk_and_back);
    RUN(test_polish_sparks_only_while_pressed);
    RUN(test_polish_trunk_stays_on_screen_and_beside_the_sheep);
    RUN(test_polish_shapes_stay_inside_their_boxes);
    RUN(test_trim_remembers_whether_the_sheep_was_grown);
    RUN(test_trim_actions_last_trim_action_ticks);
    RUN(test_pet_stroke_and_bleat_timing);
    RUN(test_pet_bounce_only_at_the_stroke);
    RUN(test_pet_heart_rises_and_leaves_the_screen);
    RUN(test_pet_heart_is_centered_over_the_sheep_and_on_screen);
    RUN(test_pet_heart_shape);
    RUN(test_pet_blips_at_the_stroke_and_bleats_after);
    RUN(test_sun_is_fixed_in_the_sky);
    RUN(test_sun_shape_and_steady);
    RUN(test_clouds_stay_in_sky_and_drift);
    RUN(test_menu_labels_for_young_and_adult);
    RUN(test_baby_and_young_menu_have_no_shear_or_polish);
    RUN(test_baby_menu_cursor_wraps_at_five);
    RUN(test_wool_and_horn_grow_only_for_adults);
    RUN(test_action_sounds_are_deferred);
    RUN(test_no_sound_when_action_has_no_effect);
    RUN(test_evolution_sound_is_deferred);
    RUN(test_young_choice_weights);
    RUN(test_choice_weights_stay_in_range_for_out_of_range_values);
    RUN(test_adult_choice_weights);
    RUN(test_pick_weighted_uses_the_whole_roll);
    RUN(test_adult_evolution_never_picks_merino);
    RUN(test_young_evolution_follows_hunger_and_happy);
    RUN(test_young_evolution_can_pick_every_family);
    RUN(test_old_save_with_tend_values_still_loads);
    RUN(test_melody_player_follows_time);
    RUN(test_melody_player_catches_up_after_a_late_update);
    RUN(test_melody_player_starting_with_a_rest);
    RUN(test_melody_player_stop_and_restart);
    RUN(test_melody_player_handles_millisecond_counter_wraparound);
    RUN(test_ending_melody_shape);
    RUN(test_death_queues_the_ending_song_and_plays_after_drawing);
    RUN(test_starvation_also_queues_the_ending_song);
    RUN(test_button_on_grave_screen_stops_the_song_and_starts_naming);
    RUN(test_ending_song_without_sound_does_not_crash);
    RUN(test_profile_opens_and_any_button_closes);
    RUN(test_profile_allowed_while_sleeping);
    RUN(test_kind_names);
    RUN(test_kind_names_and_profile_labels_renderable);
    RUN(test_age_days_counts_from_first_day);
    RUN(test_evolution_resets_menu_cursor_when_item_count_changes);
    RUN(test_evolution_clears_wool_and_horn);
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
