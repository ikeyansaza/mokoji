#pragma once
#include <cstdint>
#include "save.h"

class Sound;

class Game {
public:
    enum class Stage : uint8_t {
        LAMB, YOUNG_MOKO, YOUNG_SURA, YOUNG_RARE, ADULT
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

    Sound*     _sound;

    int        _walk_x;
    int        _walk_dir;
    int        _walk_tick;
    Face       _face;
    Action     _action;

    Screen     _screen;
    int        _menu_cursor;

    GraveRecord _graves[MAX_GRAVES];
    int         _grave_count;

    void newGame();
    void updateWalk();
    void doAction(Action act);
    void evolveYoung();
    void evolveAdult();
    void die();
};
