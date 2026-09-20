#pragma once
#include <cstdint>
#include "save.h"
#include "jump_game.h"

class Sound;

class Game {
public:
    // 進化ステージ。spec: memory/project_mokoji_evolution_spec.md
    enum class Stage : uint8_t {
        BABY,            // ベビーシープ（共通子供）
        YOUNG_MOKO,      // モコ系若羊
        YOUNG_SUFFOLK,   // サフォーク系若羊
        YOUNG_WILD,      // ワイルド系若羊
        ADULT,           // 成体（増毛期/角長期は wool/horn 量で判定）
    };
    enum class DeathCause : uint8_t {
        STATUS = 0,   // 餓死 / 不幸死
        AGE    = 1    // 天寿
    };
    // 7 品種：モコ系 3 + サフォーク系 2 + ワイルド系 2
    enum class Breed : uint8_t {
        NONE,
        MERINO, CORRIEDALE, LINCOLN,   // モコ系
        SUFFOLK, HAMPSHIRE,            // サフォーク系
        MOUFLON, BIGHORN,              // ワイルド系
    };
    // 系統（Stage / Breed から導出可能）
    enum class Family : uint8_t { NONE, MOKO, SUFFOLK, WILD };
    enum class Screen     : uint8_t { MAIN, MENU, GRAVE, NAMING, MINIGAME };
    enum class Face       : uint8_t { LEFT, FRONT, RIGHT };
    enum class Action     : uint8_t { NONE, FEED, PET, SHEAR, POLISH, MINI };
    enum class Button     : uint8_t { LEFT, CENTER, RIGHT, LEFT_LONG };
    enum class NamingMode : uint8_t {
        SELECT_MODE,   // [PRESET] / [TYPE] のどちらかを選ぶ
        PRESET_PICK,   // プリセット 20 個を左右で wheel
        INPUT_ROW,     // 手動入力：行を選ぶ（あ行/か行/.../BS/OK）
        INPUT_CHAR,    // 手動入力：行内の文字を選ぶ
    };

    static constexpr int  MENU_COUNT = 5;
    // 系統別に「CUT/POLI」が切り替わるため、メニュー項目とラベルは Game の状態を見て返す。
    Action      menuAction(int i) const;
    const char* menuLabel(int i)  const;   // UTF-8
    static const char*    breedSlug(Breed b);        // 短縮表記（4-5 文字）

    // ゲーム内時間の刻み定数。DEBUG_FAST / HOST_TEST 時は短縮版に切り替わる。
    // 公開しているのはホスト側ユニットテストや実機検証で参照するため。
#if defined(DEBUG_FAST) || defined(HOST_TEST)
    static constexpr uint32_t TICKS_PER_HOUR = 60u;                    // 1 game-hour = 3 sec real
#else
    static constexpr uint32_t TICKS_PER_HOUR = 20u * 60u * 60u;        // 1 game-hour = 1 hour real
#endif
    static constexpr uint32_t TICKS_PER_DAY  = TICKS_PER_HOUR * 24u;

    Game(Sound* sound, const GameSaveData* data = nullptr);

    void tick();
    void onButton(Button btn);
    GameSaveData saveData() const;

    // 描画用アクセサ
    Stage     stage()       const { return _stage; }
    Breed     breed()       const { return _breed; }
    Family    family()      const;   // _stage / _breed から判定
    bool      isFluffy()    const;   // 増毛期判定（モコ・サフォーク系 + wool > 閾値）
    bool      isLonghorn()  const;   // 角長期判定（ワイルド系 + horn > 閾値）
    int       horn()        const { return _horn; }
    Screen    screen()      const { return _screen; }
    Face      face()        const { return _face; }
    Action    action()      const { return _action; }
    const char* name()      const { return _name; }
    const uint8_t* nameKana() const { return _name_kana; }

    // 命名画面のアクセサ
    NamingMode namingMode()   const { return _naming_mode; }
    int        namingCursor() const { return _naming_cursor; }
    int        inputRow()     const { return _input_row; }
    int        inputLen()     const { return _input_len; }
    const uint8_t* inputBuffer() const { return _input_buffer; }
    int       hunger()      const { return _hunger; }
    int       happy()       const { return _happy; }
    int       wool()        const { return _wool; }
    bool      sleeping()    const { return _sleeping; }
    int       walkX()       const { return _walk_x; }
    int       walkTick()    const { return _walk_tick; }   // アクション中のアニメ進行用 (0..40)
    int       menuCursor()  const { return _menu_cursor; }
    bool      leftHeld()    const { return _left_held; }   // 押下中はステータス overlay を出す
    void      setLeftHeld(bool h) { _left_held = h; }      // main 側で毎フレーム反映
    int       graveCount()  const { return _grave_count; }
    const GraveRecord& grave(int i) const { return _graves[i]; }
    uint32_t  ageTicks()    const { return _age_ticks; }   // テスト用

    // Flash 寿命対策：状態が変わったときだけ true。main 側で saved 後に clearDirty()。
    // age_ticks 単独の進行は dirty を立てない（main 側の長周期 force-save が拾う）。
    bool      isDirty()     const { return _dirty; }

    // ミニゲーム「柵を跳ぶ羊」。main.cpp が現在時刻（ms）を渡し、ミニゲーム中は短い周期で
    // updateMini() を呼ぶ。ゲームの進行は JumpGame が持ち、Game は結果（幸福度・空腹）を反映する。
    void setNowMs(uint32_t now_ms) { _now_ms = now_ms; }
    void updateMini();
    bool inMiniGame() const { return _screen == Screen::MINIGAME; }
    const JumpGame& jump() const { return _jump; }
    int  miniReward() const { return _mini_reward; }   // 直近のゲームで上がった幸福度（表示用）
    void      clearDirty()        { _dirty = false; }

private:
    JumpGame   _jump;
    uint32_t   _now_ms        = 0;
    int        _mini_reward   = 0;
    void       applyMiniReward();
    void       playMiniSounds(uint8_t ev);
    char       _name[8];                  // romaji 表示用
    uint8_t    _name_kana[5];             // ひらがな index 列（kana::END 終端、最大 4 字）
    Stage      _stage;
    Breed      _breed;
    int        _hunger;
    int        _happy;
    int        _sleepy;
    int        _wool;            // モコ・サフォーク系の毛量、SHEAR で 0 リセット
    int        _horn;            // ワイルド系の角の長さ、POLISH で 0 リセット
    uint32_t   _age_ticks;
    int        _tend_feed;
    int        _tend_pet;
    int        _tend_shear;
    int        _tend_polish;     // POLISH（角研ぎ）の世話回数
    bool       _sleeping;
    uint8_t    _lifespan_days;   // 10-15 日のランダム個体寿命（自然死の上限）

    Sound*     _sound;

    int        _walk_x;
    int        _walk_dir;
    int        _walk_tick;
    Face       _face;
    Action     _action;

    // 「うろうろ」のアイドル状態機械（描画用、save 不要）
    enum class WalkState : uint8_t { WALK, PAUSE, LOOK };
    WalkState  _walk_state;
    int        _walk_state_remaining;

    // 命名画面の状態（save 不要）
    NamingMode _naming_mode;
    int        _naming_cursor;
    int        _input_row;       // INPUT_CHAR 中の所属行
    int        _input_len;       // _input_buffer に入っている文字数（0..4）
    uint8_t    _input_buffer[5]; // 構築中の名前（kana::END 終端）

    Screen     _screen;
    int        _menu_cursor;
    bool       _left_held;            // LEFT ボタンを押下中か（ステータス表示用）

    GraveRecord _graves[MAX_GRAVES];
    int         _grave_count;

    bool        _dirty;

    void newGame();
    void updateWalk();
    void doAction(Action act);
    void evolveYoung();
    void evolveAdult();
    void die(DeathCause cause);

    // 命名画面まわり
    void startNaming();
    void handleNamingButton(Button btn);
    void commitName();           // _input_buffer を _name_kana / _name に確定して MAIN へ
    void deriveRomajiName();     // _name_kana → _name を再生成
};
