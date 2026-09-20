#include "game.h"
#include "sound.h"
#include "kana.h"
#include "pico/rand.h"
#include <cstring>
#include <algorithm>

namespace {
// TICKS_PER_HOUR / TICKS_PER_DAY は Game 側に公開した（テスト・DEBUG_FAST との整合のため）
constexpr int      RARE_PROB      = 5;                   // %（実際は /128 ≒ 3.9%、Python 版と同条件）
// 羊スプライトは 2x スケール表示で 48x48 になるため、画面端まで使えるよう範囲を広げる
constexpr int      WALK_LEFT      = 0;
constexpr int      WALK_RIGHT     = 80;   // 128 - 48 = 80（右端余白ゼロ）
constexpr int      DECAY_HOURS    = 3;                   // 空腹/幸福の減少間隔（game-hour）
constexpr int      SLEEP_PET_HAPPY = 10;                  // 就寝中に撫でた時の幸福度増分（起床時 PET は +20）
constexpr int      START_HOUR     = 8;                   // 起動時のゲーム内時刻（朝 8 時）→ 即就寝を防ぐ
constexpr int      LIFESPAN_MIN   = 10;                  // 自然死 寿命下限（リアル日）
constexpr int      LIFESPAN_RANGE = 6;                   // [10, 15] のレンジ

inline uint32_t rand7() { return get_rand_32() & 0x7Fu; }
inline uint32_t rand8() { return get_rand_32() & 0xFFu; }

// メニューラベルは 8x8 ひらがな（kana index 列、END 終端）。2 倍表示で左右の < > と重ならない 4 字まで。
// index は kana.cpp の並び。打ち間違いは test_menu_labels_are_hiragana が romaji で検出する。
constexpr uint8_t kLabelEat[]    = { 50, 25, 45, kana::END };   // ごはん
constexpr uint8_t kLabelPet[]    = { 20, 59, 40, kana::END };   // なでる
constexpr uint8_t kLabelCut[]    = {  5, 40, kana::END };       // かる
constexpr uint8_t kLabelFun[]    = {  0, 14, 63, kana::END };   // あそぶ
constexpr uint8_t kLabelBack[]   = { 34, 60, 40, kana::END };   // もどる
constexpr uint8_t kLabelPolish[] = { 31, 46,  7, kana::END };   // みがく
constexpr uint8_t kLabelNone[]   = { kana::END };
const uint8_t* const kMenuLabelsDefault[Game::MENU_COUNT] = {
    kLabelEat, kLabelPet, kLabelCut, kLabelFun, kLabelBack
};
const Game::Action kMenuActionsDefault[Game::MENU_COUNT] = {
    Game::Action::FEED, Game::Action::PET, Game::Action::SHEAR, Game::Action::MINI,
    Game::Action::NONE   // BACK
};
}  // namespace

const uint8_t* Game::menuLabel(int i) const {
    if (i < 0 || i >= MENU_COUNT) return kLabelNone;
    if (i == 2 && family() == Family::WILD) return kLabelPolish;
    return kMenuLabelsDefault[i];
}

Game::Action Game::menuAction(int i) const {
    if (i < 0 || i >= MENU_COUNT) return Action::NONE;
    if (i == 2 && family() == Family::WILD) return Action::POLISH;
    return kMenuActionsDefault[i];
}

const char* Game::breedSlug(Breed b) {
    switch (b) {
        case Breed::MERINO:     return "MERI";
        case Breed::CORRIEDALE: return "CORR";
        case Breed::LINCOLN:    return "LINC";
        case Breed::SUFFOLK:    return "SUFF";
        case Breed::HAMPSHIRE:  return "HAMP";
        case Breed::MOUFLON:    return "MOUF";
        case Breed::BIGHORN:    return "BIGH";
        default:                return "??";
    }
}

Game::Family Game::family() const {
    switch (_stage) {
        case Stage::YOUNG_MOKO:    return Family::MOKO;
        case Stage::YOUNG_SUFFOLK: return Family::SUFFOLK;
        case Stage::YOUNG_WILD:    return Family::WILD;
        case Stage::ADULT:
            switch (_breed) {
                case Breed::MERINO:
                case Breed::CORRIEDALE:
                case Breed::LINCOLN:    return Family::MOKO;
                case Breed::SUFFOLK:
                case Breed::HAMPSHIRE:  return Family::SUFFOLK;
                case Breed::MOUFLON:
                case Breed::BIGHORN:    return Family::WILD;
                default:                return Family::NONE;
            }
        default:                   return Family::NONE;
    }
}

bool Game::isFluffy() const {
    if (_stage != Stage::ADULT) return false;
    Family f = family();
    return (f == Family::MOKO || f == Family::SUFFOLK) && _wool > 50;
}

bool Game::isLonghorn() const {
    if (_stage != Stage::ADULT) return false;
    return family() == Family::WILD && _horn > 50;
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
    _left_held   = false;
    _walk_state  = WalkState::WALK;
    _walk_state_remaining = 80;   // 起動直後 4 秒は歩く

    if (data && data->magic == GameSaveData::MAGIC) {
        memcpy(_name, data->name, sizeof(_name));
        _name[sizeof(_name) - 1] = '\0';
        _stage       = static_cast<Stage>(data->stage);
        _breed       = static_cast<Breed>(data->breed);
        _hunger      = data->hunger;
        _happy       = data->happy;
        _sleepy      = data->sleepy;
        _wool        = data->wool;
        _horn        = data->horn;
        _age_ticks   = data->age_ticks;
        _tend_feed   = data->tend_feed;
        _tend_pet    = data->tend_pet;
        _tend_shear  = data->tend_shear;
        _tend_polish = data->tend_polish;
        _sleeping    = data->sleeping != 0;
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
        std::memcpy(_name_kana, data->name_kana, sizeof(_name_kana));
        _dirty = false;   // フラッシュ上の内容と一致しているので clean
    } else {
        newGame();        // newGame 側で _dirty = true される
        // セーブが無い＝最初の起動なので名前を付けてもらう画面に入る
        startNaming();
    }
}

void Game::newGame() {
    // 注意：_grave_count / _graves はリセットしない（過去の墓を保持）
    // 名前は最初のプリセット「もこ」(index 34, 9) をデフォルトに
    for (int i = 0; i < 5; ++i) _name_kana[i] = kana::PRESETS[0][i];
    deriveRomajiName();
    _stage       = Stage::BABY;
    _breed       = Breed::NONE;
    _hunger      = 100;
    _happy       = 100;
    _sleepy      = 0;
    _wool        = 0;
    _horn        = 0;
    _age_ticks   = 0;
    _tend_feed   = 0;
    _tend_pet    = 0;
    _tend_shear  = 0;
    _tend_polish = 0;
    _sleeping    = false;
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
        // 系統に応じて毛 or 角を伸ばす（系統別の手入れ対象だけ伸びる仕様）
        // ワイルド系は wool 伸びない（CUT 不可なので溜まり続けてしまうバグ防止）。
        Family fam = family();
        int adult_grow = (_stage == Stage::ADULT) ? 2 : 1;
        if (fam == Family::WILD) {
            _horn = std::min(100, _horn + adult_grow);
        } else {
            // BABY (NONE) / YOUNG_MOKO / YOUNG_SUFFOLK / ADULT MOKO/SUFFOLK
            _wool = std::min(100, _wool + adult_grow);
        }

        // 睡眠度の更新は「ゲーム内 1 時間ごと」にゲート。
        // 起動時刻を朝 8 時にオフセットして boot 直後の即就寝を防ぐ。
        int hour = (int(game_hour) + START_HOUR) % 24;
        if (hour >= 22 || hour < 6) {
            _sleepy = std::min(100, _sleepy + 1);
        } else if (_sleeping) {
            _sleepy = std::max(0, _sleepy - 2);
        }

        // 傾向スコアは ×0.97/hour で減衰（直近の世話が進化判定で効きやすくなる）
        _tend_feed   = (_tend_feed   * 97) / 100;
        _tend_pet    = (_tend_pet    * 97) / 100;
        _tend_shear  = (_tend_shear  * 97) / 100;
        _tend_polish = (_tend_polish * 97) / 100;

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
    if (_stage == Stage::BABY && age_days >= 3) {
        evolveYoung();
    } else if ((_stage == Stage::YOUNG_MOKO ||
                _stage == Stage::YOUNG_SUFFOLK ||
                _stage == Stage::YOUNG_WILD) && age_days >= 7) {
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
    // 就寝中に操作できるのは MAIN / MENU のみ（墓・命名は就寝中に到達しない想定だが念のため弾く）。
    // 行動の可否は doAction 側で判定する。
    if (_sleeping && _screen != Screen::MAIN && _screen != Screen::MENU) return;

    // LEFT_LONG はバックスペース専用イベント。NAMING の入力中以外は無視。
    if (btn == Button::LEFT_LONG) {
        if (_screen == Screen::NAMING &&
            (_naming_mode == NamingMode::INPUT_ROW ||
             _naming_mode == NamingMode::INPUT_CHAR)) {
            if (_input_len > 0) {
                _input_len -= 1;
                _input_buffer[_input_len] = kana::END;
            }
            // INPUT_CHAR 中なら row 選択に戻す
            if (_naming_mode == NamingMode::INPUT_CHAR) {
                _naming_mode = NamingMode::INPUT_ROW;
                _naming_cursor = 0;
            }
        }
        return;
    }

    switch (_screen) {
        case Screen::MAIN:
            if (btn == Button::CENTER) _screen = Screen::MENU;
            // LEFT は押下中（_left_held）でステータス overlay 表示する仕組みなので
            // ここでは何もしない。
            break;
        case Screen::MENU:
            if (btn == Button::LEFT) {
                _menu_cursor = (_menu_cursor - 1 + MENU_COUNT) % MENU_COUNT;
            } else if (btn == Button::RIGHT) {
                _menu_cursor = (_menu_cursor + 1) % MENU_COUNT;
            } else if (btn == Button::CENTER) {
                doAction(menuAction(_menu_cursor));
                _screen = Screen::MAIN;
            }
            break;
        case Screen::GRAVE:
            // 墓を確認したら新しい子に名前を付ける
            startNaming();
            break;
        case Screen::NAMING:
            handleNamingButton(btn);
            break;
    }
}

void Game::doAction(Action act) {
    // 就寝中は寝顔を撫でる（PET）だけ許可する。幸福度は起きている時より控えめに上がり、
    // 羊は起きない。傾向スコア・アニメーション・効果音も触らない（起こさないため）。
    // それ以外の行動は何も起こさない。
    if (_sleeping) {
        if (act == Action::PET) {
            _happy = std::min(100, _happy + SLEEP_PET_HAPPY);
            _dirty = true;
        }
        return;
    }

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
            // モコ系・サフォーク系のみ実効。それ以外は no-op。
            if (_wool > 10) {
                _wool = 0;
                _happy = std::min(100, _happy + 10);
                _tend_shear += 1;
                _action = Action::SHEAR;
                _dirty = true;
                if (_sound) _sound->joki();
            }
            break;
        case Action::POLISH:
            // ワイルド系のみ実効（角を磨いて短くする）
            if (_horn > 10) {
                _horn = 0;
                _happy = std::min(100, _happy + 10);
                _tend_polish += 1;
                _action = Action::POLISH;
                _dirty = true;
                if (_sound) _sound->joki();   // 暫定で同じ音
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
    // BABY → 3 系統。世話パターンで重み付け：
    //   モコ系：feed + pet 多め（健康重視）
    //   サフォーク系：pet 多め（人懐っこい）
    //   ワイルド系：放置気味（feed/pet 少なめ → 残りに割り振り）
    int total = _tend_feed + _tend_pet + _tend_shear + 1;
    int moko_w    = 50 + (_tend_feed + _tend_pet) * 30 / total;
    int suffolk_w = 50 + _tend_pet * 40 / total;
    int wild_w    = 50;   // ベース確率、世話少ないと相対的に上がる

    int roll = rand7() % (moko_w + suffolk_w + wild_w);
    if (roll < moko_w) {
        _stage = Stage::YOUNG_MOKO;
    } else if (roll < moko_w + suffolk_w) {
        _stage = Stage::YOUNG_SUFFOLK;
    } else {
        _stage = Stage::YOUNG_WILD;
        _wool = 0;   // ワイルド系は wool 概念がない、BABY 時代の蓄積をリセット
    }
    _dirty = true;
    if (_sound) _sound->happy();
}

void Game::evolveAdult() {
    int total = _tend_feed + _tend_pet + _tend_shear + _tend_polish + 1;

    if (_stage == Stage::YOUNG_MOKO) {
        // モコ系：feed 多めで MERINO（毛量重視）、pet 多めで CORRIEDALE、shear 多めで LINCOLN（長毛）
        int merino_w     = 50 + _tend_feed  * 50 / total;
        int corriedale_w = 50 + _tend_pet   * 50 / total;
        int lincoln_w    = 50 + _tend_shear * 50 / total;
        int roll = rand8() % (merino_w + corriedale_w + lincoln_w);
        if (roll < merino_w)                           _breed = Breed::MERINO;
        else if (roll < merino_w + corriedale_w)       _breed = Breed::CORRIEDALE;
        else                                           _breed = Breed::LINCOLN;
    } else if (_stage == Stage::YOUNG_SUFFOLK) {
        // サフォーク系：pet 多めで SUFFOLK（人懐っこい）、feed+pet で HAMPSHIRE（強化版）
        int suffolk_w   = 50 + _tend_pet * 50 / total;
        int hampshire_w = 50 + (_tend_feed + _tend_pet) * 30 / total;
        int roll = rand8() % (suffolk_w + hampshire_w);
        _breed = (roll < suffolk_w) ? Breed::SUFFOLK : Breed::HAMPSHIRE;
    } else {
        // ワイルド系：polish 多めで BIGHORN（角ケア重視）、放置で MOUFLON（小型）
        int mouflon_w = 50;
        int bighorn_w = 50 + _tend_polish * 60 / total;
        int roll = rand8() % (mouflon_w + bighorn_w);
        _breed = (roll < mouflon_w) ? Breed::MOUFLON : Breed::BIGHORN;
    }

    _stage = Stage::ADULT;
    _dirty = true;
    if (_sound) _sound->happy();
}

void Game::die(DeathCause cause) {
    GraveRecord rec{};
    std::memcpy(rec.name, _name, sizeof(rec.name));
    std::memcpy(rec.name_kana, _name_kana, sizeof(rec.name_kana));
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
    d.stage       = uint8_t(_stage);
    d.breed       = uint8_t(_breed);
    d.hunger      = uint8_t(_hunger);
    d.happy       = uint8_t(_happy);
    d.sleepy      = uint8_t(_sleepy);
    d.wool        = uint8_t(_wool);
    d.horn        = uint8_t(_horn);
    d.sleeping    = _sleeping ? 1 : 0;
    d.age_ticks   = _age_ticks;
    d.tend_feed   = int16_t(_tend_feed);
    d.tend_pet    = int16_t(_tend_pet);
    d.tend_shear  = int16_t(_tend_shear);
    d.tend_polish = int16_t(_tend_polish);
    d.grave_count = uint8_t(_grave_count);
    d.lifespan_days = _lifespan_days;
    std::memcpy(d.name_kana, _name_kana, sizeof(d.name_kana));
    for (int i = 0; i < _grave_count && i < MAX_GRAVES; ++i) {
        d.graves[i] = _graves[i];
    }
    return d;
}

// ====================================================================
// 命名画面まわり
// ====================================================================

void Game::deriveRomajiName() {
    // _name_kana から _name (romaji) を再生成
    kana::toRomaji(_name_kana, _name, sizeof(_name));
}

void Game::startNaming() {
    _screen = Screen::NAMING;
    _naming_mode = NamingMode::SELECT_MODE;
    _naming_cursor = 0;
    _input_row = 0;
    _input_len = 0;
    for (int i = 0; i < 5; ++i) _input_buffer[i] = kana::END;
}

void Game::commitName() {
    // _input_buffer の長さを確定（END 終端を入れる）
    if (_input_len < 4) _input_buffer[_input_len] = kana::END;
    // 全部空なら確定しない
    if (_input_buffer[0] == kana::END) return;

    std::memcpy(_name_kana, _input_buffer, sizeof(_name_kana));
    deriveRomajiName();
    _screen = Screen::MAIN;
    _dirty = true;
}

void Game::handleNamingButton(Button btn) {
    switch (_naming_mode) {
        case NamingMode::SELECT_MODE: {
            // [PRESET] / [TYPE] のトグル
            if (btn == Button::LEFT)        _naming_cursor = 0;
            else if (btn == Button::RIGHT)  _naming_cursor = 1;
            else if (btn == Button::CENTER) {
                if (_naming_cursor == 0) {
                    _naming_mode = NamingMode::PRESET_PICK;
                    _naming_cursor = 0;
                } else {
                    _naming_mode = NamingMode::INPUT_ROW;
                    _naming_cursor = 0;
                    _input_len = 0;
                    for (int i = 0; i < 5; ++i) _input_buffer[i] = kana::END;
                }
            }
            break;
        }
        case NamingMode::PRESET_PICK: {
            if (btn == Button::LEFT)
                _naming_cursor = (_naming_cursor - 1 + kana::PRESET_COUNT) % kana::PRESET_COUNT;
            else if (btn == Button::RIGHT)
                _naming_cursor = (_naming_cursor + 1) % kana::PRESET_COUNT;
            else if (btn == Button::CENTER) {
                std::memcpy(_input_buffer, kana::PRESETS[_naming_cursor], 5);
                // _input_len はプリセットの実長（END まで数える）
                _input_len = 0;
                for (int i = 0; i < 4; ++i) {
                    if (_input_buffer[i] == kana::END) break;
                    ++_input_len;
                }
                commitName();
            }
            break;
        }
        case NamingMode::INPUT_ROW: {
            // ROW_COUNT 行 + BS + OK = ROW_COUNT + 2
            const int total = kana::ROW_COUNT + 2;
            if (btn == Button::LEFT)
                _naming_cursor = (_naming_cursor - 1 + total) % total;
            else if (btn == Button::RIGHT)
                _naming_cursor = (_naming_cursor + 1) % total;
            else if (btn == Button::CENTER) {
                if (_naming_cursor < kana::ROW_COUNT) {
                    _input_row = _naming_cursor;
                    _naming_mode = NamingMode::INPUT_CHAR;
                    _naming_cursor = 0;
                } else if (_naming_cursor == kana::ROW_COUNT) {
                    // BS：1 文字削除
                    if (_input_len > 0) {
                        _input_len -= 1;
                        _input_buffer[_input_len] = kana::END;
                    }
                } else {
                    // OK：確定
                    commitName();
                }
            }
            break;
        }
        case NamingMode::INPUT_CHAR: {
            const auto& row = kana::ROWS[_input_row];
            if (btn == Button::LEFT)
                _naming_cursor = (_naming_cursor - 1 + row.length) % row.length;
            else if (btn == Button::RIGHT)
                _naming_cursor = (_naming_cursor + 1) % row.length;
            else if (btn == Button::CENTER) {
                if (_input_len < 4) {
                    _input_buffer[_input_len++] = uint8_t(row.start + _naming_cursor);
                    if (_input_len < 4) _input_buffer[_input_len] = kana::END;
                }
                _naming_mode = NamingMode::INPUT_ROW;
                _naming_cursor = 0;
            }
            break;
        }
    }
}
