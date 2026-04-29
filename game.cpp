#include "game.h"
#include "sound.h"
#include "pico/rand.h"
#include <cstring>
#include <algorithm>

namespace {
// TICKS_PER_HOUR / TICKS_PER_DAY は Game 側に公開した（テスト・DEBUG_FAST との整合のため）
constexpr int      RARE_PROB      = 5;                   // %（実際は /128 ≒ 3.9%、Python 版と同条件）
constexpr int      WALK_LEFT      = 10;
constexpr int      WALK_RIGHT     = 90;
constexpr int      DECAY_HOURS    = 3;                   // 空腹/幸福の減少間隔（game-hour）
constexpr int      START_HOUR     = 8;                   // 起動時のゲーム内時刻（朝 8 時）→ 即就寝を防ぐ
constexpr int      LIFESPAN_MIN   = 10;                  // 自然死 寿命下限（リアル日）
constexpr int      LIFESPAN_RANGE = 6;                   // [10, 15] のレンジ

inline uint32_t rand7() { return get_rand_32() & 0x7Fu; }
inline uint32_t rand8() { return get_rand_32() & 0xFFu; }

const char* const kMenuLabels[Game::MENU_COUNT] = { "EAT", "PET", "CUT", "FUN" };
}  // namespace

const Game::Action Game::MENU_ITEMS[Game::MENU_COUNT] = {
    Action::FEED, Action::PET, Action::SHEAR, Action::MINI
};

const char* Game::menuLabel(int i) {
    if (i < 0 || i >= MENU_COUNT) return "";
    return kMenuLabels[i];
}

const char* Game::breedSlug(Breed b) {
    switch (b) {
        case Breed::CORRIEDALE:   return "CORR";
        case Breed::MERINO:       return "MERI";
        case Breed::SUFFOLK:      return "SUFF";
        case Breed::SOUTHDOWN:    return "SOUT";
        case Breed::EASTFRIESIAN: return "EAST";
        default:                  return "??";
    }
}

Game::Game(Sound* sound, const GameSaveData* data)
    : _sound(sound) {
    // 墓は常に保持（newGame では触らない仕様）
    _grave_count = 0;
    _walk_x      = 52;
    _walk_dir    = 1;
    _walk_tick   = 0;
    _face        = Face::RIGHT;
    _action      = Action::NONE;
    _screen      = Screen::MAIN;
    _menu_cursor = 0;
    _walk_state  = WalkState::WALK;
    _walk_state_remaining = 80;   // 起動直後 4 秒は歩く

    if (data && data->magic == GameSaveData::MAGIC) {
        memcpy(_name, data->name, sizeof(_name));
        _name[sizeof(_name) - 1] = '\0';
        _stage      = static_cast<Stage>(data->stage);
        _breed      = static_cast<Breed>(data->breed);
        _sheep_type = static_cast<SheepType>(data->sheep_type);
        _hunger     = data->hunger;
        _happy      = data->happy;
        _sleepy     = data->sleepy;
        _wool       = data->wool;
        _age_ticks  = data->age_ticks;
        _tend_feed  = data->tend_feed;
        _tend_pet   = data->tend_pet;
        _tend_shear = data->tend_shear;
        _sleeping   = data->sleeping != 0;
        _grave_count = data->grave_count;
        if (_grave_count > MAX_GRAVES) _grave_count = MAX_GRAVES;
        for (int i = 0; i < _grave_count; ++i) {
            _graves[i] = data->graves[i];
        }
        _lifespan_days = data->lifespan_days;
        if (_lifespan_days == 0) {
            // 旧 magic からマイグレートされた場合は新規に乱数で割り当て
            _lifespan_days = uint8_t(LIFESPAN_MIN + (get_rand_32() % LIFESPAN_RANGE));
        }
        _dirty = false;   // フラッシュ上の内容と一致しているので clean
    } else {
        newGame();        // newGame 側で _dirty = true される
    }
}

void Game::newGame() {
    // 注意：_grave_count / _graves はリセットしない（過去の墓を保持）
    std::strncpy(_name, "MOKO", sizeof(_name) - 1);
    _name[sizeof(_name) - 1] = '\0';
    _stage      = Stage::LAMB;
    _breed      = Breed::NONE;
    _sheep_type = SheepType::NONE;
    _hunger     = 100;
    _happy      = 100;
    _sleepy     = 0;
    _wool       = 0;
    _age_ticks  = 0;
    _tend_feed  = 0;
    _tend_pet   = 0;
    _tend_shear = 0;
    _sleeping   = false;
    _lifespan_days = uint8_t(LIFESPAN_MIN + (get_rand_32() % LIFESPAN_RANGE));  // 10-15 日
    _dirty      = true;   // 新規開始 / 死亡からの再スタート時は最初の保存を促す
}

void Game::tick() {
    _age_ticks += 1;

    // 睡眠中はステータス減少を止める（Python の int(1*0.2)==0 に対応）
    int dec = _sleeping ? 0 : 1;
    if (_age_ticks % TICKS_PER_HOUR == 0) {
        uint32_t game_hour = _age_ticks / TICKS_PER_HOUR;

        // 空腹/幸福は DECAY_HOURS おきに -1（24/day → 8/day で 10-15 日寿命に整合）
        if (game_hour % DECAY_HOURS == 0) {
            _hunger = std::max(0, _hunger - dec);
            _happy  = std::max(0, _happy  - dec);
        }
        _wool = std::min(100, _wool + 2);

        // 睡眠度の更新は「ゲーム内 1 時間ごと」にゲート。
        // 起動時刻を朝 8 時にオフセットして boot 直後の即就寝を防ぐ。
        int hour = (int(game_hour) + START_HOUR) % 24;
        if (hour >= 22 || hour < 6) {
            _sleepy = std::min(100, _sleepy + 1);
        } else if (_sleeping) {
            _sleepy = std::max(0, _sleepy - 2);
        }

        // 傾向スコアは ×0.97/hour で減衰（直近の世話が進化判定で効きやすくなる）
        _tend_feed  = (_tend_feed  * 97) / 100;
        _tend_pet   = (_tend_pet   * 97) / 100;
        _tend_shear = (_tend_shear * 97) / 100;

        _dirty = true;
    }

    if (!_sleeping && _sleepy >= 80) _sleeping = true;
    if (_sleeping  && _sleepy <= 10) _sleeping = false;

    // 自然死（寿命到達）と care-based 死亡（hunger/happy 限界）
    uint32_t age_days_now = _age_ticks / TICKS_PER_DAY;
    if (age_days_now >= _lifespan_days) {
        die(DeathCause::AGE);
        return;
    }
    if (_hunger <= 0 || _happy <= 0) {
        die(DeathCause::STATUS);
        return;
    }

    uint32_t age_days = _age_ticks / TICKS_PER_DAY;
    if (_stage == Stage::LAMB && age_days >= 3) {
        evolveYoung();
    } else if ((_stage == Stage::YOUNG_MOKO ||
                _stage == Stage::YOUNG_SURA ||
                _stage == Stage::YOUNG_RARE) && age_days >= 7) {
        evolveAdult();
    }

    if (!_sleeping) updateWalk();
}

void Game::updateWalk() {
    _walk_tick += 1;

    // メニューからのアクション中は正面を向いて固定
    if (_action != Action::NONE) {
        _face = Face::FRONT;
        if (_walk_tick > 40) _action = Action::NONE;
        return;
    }

    // アイドル状態の残り時間を消化、切れたら次の状態を抽選
    _walk_state_remaining -= 1;
    if (_walk_state_remaining <= 0) {
        uint32_t r = get_rand_32() % 100;
        if (r < 30) {
            // 30%: 立ち止まる（1-3 秒）
            _walk_state = WalkState::PAUSE;
            _walk_state_remaining = 20 + int(get_rand_32() % 40);
        } else if (r < 45) {
            // 15%: 周りを見回す（3 秒、左→正面→右→正面）
            _walk_state = WalkState::LOOK;
            _walk_state_remaining = 60;
        } else {
            // 55%: 歩く（2-6 秒）。確率で進行方向を反転、左右の壁感を弱める。
            _walk_state = WalkState::WALK;
            _walk_state_remaining = 40 + int(get_rand_32() % 80);
            if (r < 60) _walk_dir = -_walk_dir;
        }
    }

    switch (_walk_state) {
        case WalkState::WALK:
            if (_walk_tick % 3 == 0) {
                _walk_x += _walk_dir;
                _face = (_walk_dir > 0) ? Face::RIGHT : Face::LEFT;
                if      (_walk_x > WALK_RIGHT) _walk_dir = -1;
                else if (_walk_x < WALK_LEFT)  _walk_dir =  1;
            }
            break;
        case WalkState::PAUSE:
            _face = Face::FRONT;
            break;
        case WalkState::LOOK: {
            // 残り時間 60→0 を 4 段階（15 tick ずつ）に切って首振り
            int phase = (60 - _walk_state_remaining) / 15;
            switch (phase) {
                case 0:  _face = Face::LEFT;  break;
                case 1:  _face = Face::FRONT; break;
                case 2:  _face = Face::RIGHT; break;
                default: _face = Face::FRONT; break;
            }
            break;
        }
    }
}

void Game::onButton(Button btn) {
    if (_sleeping) return;

    switch (_screen) {
        case Screen::MAIN:
            if (btn == Button::CENTER) _screen = Screen::MENU;
            break;
        case Screen::MENU:
            if (btn == Button::LEFT) {
                _menu_cursor = (_menu_cursor - 1 + MENU_COUNT) % MENU_COUNT;
            } else if (btn == Button::RIGHT) {
                _menu_cursor = (_menu_cursor + 1) % MENU_COUNT;
            } else if (btn == Button::CENTER) {
                doAction(MENU_ITEMS[_menu_cursor]);
                _screen = Screen::MAIN;
            }
            break;
        case Screen::GRAVE:
            // Python 版にあったデッドエンドを修正：どのボタンでも main に戻る
            _screen = Screen::MAIN;
            break;
    }
}

void Game::doAction(Action act) {
    switch (act) {
        case Action::FEED:
            _hunger = std::min(100, _hunger + 30);
            _tend_feed += 1;
            _action = Action::FEED;
            _dirty = true;
            if (_sound) _sound->mog();
            break;
        case Action::PET:
            _happy = std::min(100, _happy + 20);
            _tend_pet += 1;
            _action = Action::PET;
            _dirty = true;
            if (_sound) _sound->mee();
            break;
        case Action::SHEAR:
            if (_wool > 10) {
                _wool = 0;
                _happy = std::min(100, _happy + 10);
                _tend_shear += 1;
                _action = Action::SHEAR;
                _dirty = true;
                if (_sound) _sound->joki();
            }
            break;
        case Action::MINI:
            // ミニゲーム未実装。仮で happy +5 とハッピー音
            _happy = std::min(100, _happy + 5);
            _action = Action::MINI;
            _dirty = true;
            if (_sound) _sound->happy();
            break;
        default:
            break;
    }
    _walk_tick = 0;
}

void Game::evolveYoung() {
    if (rand7() < uint32_t(RARE_PROB)) {
        _stage = Stage::YOUNG_RARE;
        _sheep_type = SheepType::RARE;
        _dirty = true;
        if (_sound) _sound->happy();
        return;
    }

    int total = _tend_feed + _tend_pet + _tend_shear + 1;
    int moko_w = 50 + (_tend_feed + _tend_pet) * 30 / total;
    int sura_w = 50 + _tend_shear * 30 / total;

    int roll = rand7() % (moko_w + sura_w);
    if (roll < moko_w) {
        _stage = Stage::YOUNG_MOKO;
        _sheep_type = SheepType::MOKO;
    } else {
        _stage = Stage::YOUNG_SURA;
        _sheep_type = SheepType::SURA;
    }
    _dirty = true;
    if (_sound) _sound->happy();
}

void Game::evolveAdult() {
    int total = _tend_feed + _tend_pet + _tend_shear + 1;

    if (_sheep_type == SheepType::MOKO) {
        int cor_w = 50 + _tend_feed * 50 / total;
        int mer_w = 50 + _tend_pet  * 50 / total;
        int roll = rand8() % (cor_w + mer_w);
        _breed = (roll < cor_w) ? Breed::CORRIEDALE : Breed::MERINO;
    } else if (_sheep_type == SheepType::SURA) {
        int suf_w = 50 + (_tend_feed + _tend_pet) * 30 / total;
        int sou_w = 50 + _tend_shear * 50 / total;
        int roll = rand8() % (suf_w + sou_w);
        _breed = (roll < suf_w) ? Breed::SUFFOLK : Breed::SOUTHDOWN;
    } else {
        _breed = Breed::EASTFRIESIAN;
    }

    _stage = Stage::ADULT;
    _dirty = true;
    if (_sound) _sound->happy();
}

void Game::die(DeathCause cause) {
    GraveRecord rec{};
    std::memcpy(rec.name, _name, sizeof(rec.name));
    const char* slug = (_breed != Breed::NONE) ? breedSlug(_breed) : "??";
    std::strncpy(rec.breed, slug, sizeof(rec.breed) - 1);
    rec.breed[sizeof(rec.breed) - 1] = '\0';
    uint32_t age_days = _age_ticks / TICKS_PER_DAY;
    rec.age_days = uint8_t(age_days > 255 ? 255 : age_days);
    rec.death_cause = uint8_t(cause);

    if (_grave_count < MAX_GRAVES) {
        _graves[_grave_count++] = rec;
    } else {
        // FIFO：最古を捨てて末尾に追加
        for (int i = 0; i < MAX_GRAVES - 1; ++i) _graves[i] = _graves[i + 1];
        _graves[MAX_GRAVES - 1] = rec;
    }
    newGame();              // 名前・ステータスをリセット（_graves は保持）
    _screen = Screen::GRAVE;
}

GameSaveData Game::saveData() const {
    GameSaveData d{};
    d.magic = GameSaveData::MAGIC;
    std::memcpy(d.name, _name, sizeof(_name));
    d.stage      = uint8_t(_stage);
    d.breed      = uint8_t(_breed);
    d.sheep_type = uint8_t(_sheep_type);
    d.hunger     = uint8_t(_hunger);
    d.happy      = uint8_t(_happy);
    d.sleepy     = uint8_t(_sleepy);
    d.wool       = uint8_t(_wool);
    d.sleeping   = _sleeping ? 1 : 0;
    d.age_ticks  = _age_ticks;
    d.tend_feed  = int16_t(_tend_feed);
    d.tend_pet   = int16_t(_tend_pet);
    d.tend_shear = int16_t(_tend_shear);
    d.grave_count = uint8_t(_grave_count);
    d.lifespan_days = _lifespan_days;
    for (int i = 0; i < _grave_count && i < MAX_GRAVES; ++i) {
        d.graves[i] = _graves[i];
    }
    return d;
}
