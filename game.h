#pragma once
#include <cstdint>
#include "save.h"

class Sound;

class Game {
public:
    enum class Stage : uint8_t {
        LAMB, YOUNG_MOKO, YOUNG_SURA, YOUNG_RARE, ADULT
    };
    enum class DeathCause : uint8_t {
        STATUS = 0,   // 餓死 / 不幸死
        AGE    = 1    // 天寿
    };
    enum class Breed : uint8_t {
        NONE, CORRIEDALE, MERINO, SUFFOLK, SOUTHDOWN, EASTFRIESIAN
    };
    enum class SheepType : uint8_t { NONE, MOKO, SURA, RARE };
    enum class Screen    : uint8_t { MAIN, MENU, GRAVE };
    enum class Face      : uint8_t { LEFT, FRONT, RIGHT };
    enum class Action    : uint8_t { NONE, FEED, PET, SHEAR, MINI };
    enum class Button    : uint8_t { LEFT, CENTER, RIGHT };

    static constexpr int  MENU_COUNT = 4;
    static const Action   MENU_ITEMS[MENU_COUNT];
    static const char*    menuLabel(int i);          // "EAT" / "PET" / "CUT" / "FUN"
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
    SheepType sheepType()   const { return _sheep_type; }
    Screen    screen()      const { return _screen; }
    Face      face()        const { return _face; }
    Action    action()      const { return _action; }
    const char* name()      const { return _name; }
    int       hunger()      const { return _hunger; }
    int       happy()       const { return _happy; }
    int       wool()        const { return _wool; }
    bool      sleeping()    const { return _sleeping; }
    int       walkX()       const { return _walk_x; }
    int       menuCursor()  const { return _menu_cursor; }
    int       graveCount()  const { return _grave_count; }
    const GraveRecord& grave(int i) const { return _graves[i]; }
    uint32_t  ageTicks()    const { return _age_ticks; }   // テスト用

    // Flash 寿命対策：状態が変わったときだけ true。main 側で saved 後に clearDirty()。
    // age_ticks 単独の進行は dirty を立てない（main 側の長周期 force-save が拾う）。
    bool      isDirty()     const { return _dirty; }
    void      clearDirty()        { _dirty = false; }

private:
    char       _name[8];
    Stage      _stage;
    Breed      _breed;
    SheepType  _sheep_type;
    int        _hunger;
    int        _happy;
    int        _sleepy;
    int        _wool;
    uint32_t   _age_ticks;
    int        _tend_feed;
    int        _tend_pet;
    int        _tend_shear;
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
};
