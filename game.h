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

    // 進化の候補と重み。進化のときの空腹（hunger）・幸福（happy）の値（0〜100）だけで重みを決め、
    // 残りは乱数で選ぶ。乱数と切り離した純粋関数にしてあるので、テストで重みを直接確かめられる。
    struct EvoChoice {
        Stage stage;    // 進化後の成長段階
        Breed breed;    // 進化後の品種（ベビー → 若羊では NONE）
        int   weight;   // 選ばれやすさ
    };
    // ベビー → 若羊。モコ系・サフォーク系・ワイルド系の 3 つ（この順）を out に書く。
    static void youngChoices(int hunger, int happy, EvoChoice out[3]);
    // 若羊 → 成体。若羊の段階（YOUNG_*）から、その系統の 2 品種を out に書いて、個数を返す（若羊でなければ 0）。
    static int  adultChoices(Stage young, int hunger, int happy, EvoChoice out[2]);
    // 重みに従って 1 つ選ぶ。roll は乱数（roll % 重みの合計 で選ぶ）。合計が 0 以下なら 0 番目。
    static int  pickWeighted(const EvoChoice* choices, int n, uint32_t roll);
    enum class Screen     : uint8_t { MAIN, MENU, GRAVE, NAMING, MINIGAME, PROFILE };
    enum class Face       : uint8_t { LEFT, FRONT, RIGHT };
    enum class Action     : uint8_t { NONE, FEED, PET, SHEAR, POLISH, MINI, PROFILE };
    // 待つ音（sleep_ms で鳴り終わるまで止まる音）。ボタン処理の中で鳴らすと、鳴り終わるまで画面の
    // 描画が遅れ、メニューが表示されたまま音が聞こえる。そこで Game は予約するだけにして、
    // main が描画のあとに playPendingSfx() で鳴らす。ミニゲームの音（blip）は待たないので、その場で鳴らす。
    enum class Sfx        : uint8_t { NONE, MOG, MEE, JOKI, SNIP, RUB, HAPPY, ENDING };
    enum class Button     : uint8_t { LEFT, CENTER, RIGHT, LEFT_LONG };
    enum class NamingMode : uint8_t {
        SELECT_MODE,   // [PRESET] / [TYPE] のどちらかを選ぶ
        PRESET_PICK,   // プリセット 20 個を左右で wheel
        INPUT_ROW,     // 手動入力：行を選ぶ（あ行/か行/.../BS/OK）
        INPUT_CHAR,    // 手動入力：行内の文字を選ぶ
    };

    static constexpr int  MENU_COUNT = 6;
    // アクション（撫でる・毛刈りなど）の動きの長さ（フレーム、1 フレーム約 50ms）。
    // ご飯は、ぱくっを 1 秒おきに 3 回するので、他より長い。
    static constexpr int  ACTION_TICKS      = 40;   // 2 秒
    static constexpr int  FEED_ACTION_TICKS = 60;   // 3 秒
    // 毛刈り（はさみが 2 往復）・角研ぎ（幹にこする）は、ご飯より短い。音は動きに合わせて鳴る。
    static constexpr int  TRIM_ACTION_TICKS = 48;   // 2.4 秒
    // 系統別に「CUT/POLI」が切り替わるため、メニュー項目とラベルは Game の状態を見て返す。
    // 毛刈り・角研ぎは成体だけ。ベビーと若羊は、メニューが 1 項目少ない（MENU_COUNT - 1）。
    int         menuCount() const;
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
#if defined(DEBUG_FAST) || defined(HOST_TEST)
    // 検証用：姿を進化の順（ベビー → モコ系の若羊・コリデール・リンカーン → サフォーク系 → ワイルド系）に
    // 1 つ進める。最後の次はベビーへ戻る。メイン画面で LEFT 長押し。
    void debugNextForm();
#endif
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
    // 毛刈り・角研ぎを始めたときの羊が、ふさふさ・角長の絵だったか。始めた瞬間に毛・角は 0 に戻るので、
    // 動きの途中まで元の絵を見せて、刈り終えたところで通常の絵に切り替えるために覚えておく。
    bool      actionWasGrown() const { return _action_was_grown; }
    const char* name()      const { return _name; }
    const uint8_t* nameKana() const { return _name_kana; }

    // 命名画面のアクセサ
    NamingMode namingMode()   const { return _naming_mode; }
    int        namingCursor() const { return _naming_cursor; }
    int        inputRow()     const { return _input_row; }
    int        inputLen()     const { return _input_len; }
    const uint8_t* inputBuffer() const { return _input_buffer; }
    // 空腹・幸福（0〜100）を 5 段階のバーにする段階（0〜5）。生きている（1 以上）なら必ず 1 以上。
    static int statusLevel(int value);
    // プロフィール画面用。種類は成長段階と品種から決まる日本語名（UTF-8）、日数は生まれた日を 1 日めと数える。
    const char* kindName() const;
    int       ageDays() const { return int(_age_ticks / TICKS_PER_DAY) + 1; }
    int       hunger()      const { return _hunger; }
    int       happy()       const { return _happy; }
    int       wool()        const { return _wool; }
    // ゲーム内の時刻（0〜23 時）。起動時は朝 8 時。22 時〜6 時が夜（就寝の判定と背景の空に使う）。
    int       hourOfDay()   const;
    bool      isNight()     const;
    bool      sleeping()    const { return _sleeping; }
    int       walkX()       const { return _walk_x; }
    int       walkTick()    const { return _walk_tick; }   // アクション中のアニメ進行用 (0..ACTION_TICKS、ご飯は 0..FEED_ACTION_TICKS)
    int       menuCursor()  const { return _menu_cursor; }
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
    Sfx  pendingSfx() const { return _pending_sfx; }
    void playPendingSfx();   // 予約された待つ音を鳴らして、予約を空にする（main が描画のあとに呼ぶ）
    bool inMiniGame() const { return _screen == Screen::MINIGAME; }
    const JumpGame& jump() const { return _jump; }
    int  miniReward() const { return _mini_reward; }   // 直近のゲームで上がった幸福度（表示用）
    void      clearDirty()        { _dirty = false; }

private:
    JumpGame   _jump;
    uint32_t   _now_ms        = 0;
    int        _mini_reward   = 0;
    void       applyMiniReward();
    Sfx        _pending_sfx = Sfx::NONE;
    void       petCues();      // 撫でるの動きの進行に合わせて、音を出す
    void       shearCues();    // 毛刈りの動きの進行に合わせて、音を予約する
    void       polishCues();   // 角研ぎの動きの進行に合わせて、音を予約する
    void       queueSfx(Sfx s) { _pending_sfx = s; }   // 後から予約したものを優先する
    void       playMiniSounds(uint8_t ev);
    char       _name[8];                  // romaji 表示用
    uint8_t    _name_kana[5];             // ひらがな index 列（kana::END 終端、最大 4 字）
    Stage      _stage;
    Breed      _breed;
    int        _hunger;
    int        _happy;
    int        _sleepy;
    int        _wool;            // モコ・サフォーク系（成体）の毛量、SHEAR で 0 リセット
    int        _horn;            // ワイルド系（成体）の角の長さ、POLISH で 0 リセット
    uint32_t   _age_ticks;
    bool       _sleeping;
    uint8_t    _lifespan_days;   // 10-15 日のランダム個体寿命（自然死の上限）

    Sound*     _sound;

    int        _walk_x;
    int        _walk_dir;
    int        _walk_tick;
    Face       _face;
    Action     _action;
    bool       _action_was_grown = false;

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
