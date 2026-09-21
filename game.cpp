#include "game.h"
#include "sound.h"
#include "kana.h"
#include "pet_motion.h"
#include "shear_motion.h"
#include "polish_motion.h"
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
constexpr int      MINI_REWARD_MAX       = 30;   // ミニゲーム 1 回で上がる幸福度の上限
constexpr int      MINI_REWARD_PER_SHEEP = 2;    // 柵を 1 つ越えるごとの幸福度
constexpr int      MINI_HUNGER_COST      = 5;    // 運動でお腹が減る量
constexpr int      START_HOUR     = 8;                   // 起動時のゲーム内時刻（朝 8 時）→ 即就寝を防ぐ
constexpr int      LIFESPAN_MIN   = 10;                  // 自然死 寿命下限（リアル日）
constexpr int      LIFESPAN_RANGE = 6;                   // [10, 15] のレンジ

// 進化の重み: 基本 50 に、空腹・幸福の値（0〜100）に応じた分を足す（係数はパーセント）。
constexpr int EVO_BASE_WEIGHT         = 50;
constexpr int EVO_MOKO_HUNGER_PCT     = 40;   // モコ系: 満腹なほど
constexpr int EVO_SUFFOLK_HAPPY_PCT   = 40;   // サフォーク系: 幸福なほど
constexpr int EVO_WILD_LOW_PCT        = 20;   // ワイルド系: 空腹・不幸なほど（(200 − hunger − happy) に掛ける）
constexpr int EVO_HAMPSHIRE_HAPPY_PCT = 50;   // HAMPSHIRE: 幸福なほど
constexpr int EVO_BIGHORN_HUNGER_PCT  = 50;   // BIGHORN: 満腹なほど

int clampPercent(int v) { return v < 0 ? 0 : (v > 100 ? 100 : v); }

// メニューラベル（UTF-8）。選択中の 1 項目を 2 倍（16px/字）で中央に出すので、左右の < > と
// 重ならない幅（5 字 = 80px）までに収める。全字が ja_font.h に収録されていることは
// test_menu_labels_renderable_and_bounded が検証する。
const char* const kMenuLabelsDefault[Game::MENU_COUNT] = {
    "ごはん", "なでる", "毛刈り", "ゲーム", "プロフ", "もどる"
};
const Game::Action kMenuActionsDefault[Game::MENU_COUNT] = {
    Game::Action::FEED, Game::Action::PET, Game::Action::SHEAR, Game::Action::MINI,
    Game::Action::PROFILE,
    Game::Action::NONE   // BACK
};
}  // namespace

namespace {
// 表示位置 i を、既定のメニュー定義（kMenu*Default）の添字に直す。
// 毛刈り・角研ぎ（添字 2）は成体だけ。ベビーと若羊は出さないので、2 番目以降を 1 つ後ろへずらす。
int menuSlot(Game::Stage stage, int i) {
    return (stage != Game::Stage::ADULT && i >= 2) ? i + 1 : i;
}
}  // namespace

int Game::hourOfDay() const {
    return int((_age_ticks / TICKS_PER_HOUR + START_HOUR) % 24);
}

bool Game::isNight() const {
    int hour = hourOfDay();
    return hour >= 22 || hour < 6;
}

int Game::statusLevel(int value) {
    if (value <= 0) return 0;
    int level = (value + 19) / 20;   // 切り上げ：1〜20 → 1、21〜40 → 2、…、81〜100 → 5
    return level > 5 ? 5 : level;
}

int Game::menuCount() const {
    return (_stage == Stage::ADULT) ? MENU_COUNT : MENU_COUNT - 1;
}

const char* Game::menuLabel(int i) const {
    if (i < 0 || i >= menuCount()) return "";
    int slot = menuSlot(_stage, i);
    if (slot == 2 && family() == Family::WILD) return "角研ぎ";
    return kMenuLabelsDefault[slot];
}

Game::Action Game::menuAction(int i) const {
    if (i < 0 || i >= menuCount()) return Action::NONE;
    int slot = menuSlot(_stage, i);
    if (slot == 2 && family() == Family::WILD) return Action::POLISH;
    return kMenuActionsDefault[slot];
}

void Game::youngChoices(int hunger, int happy, EvoChoice out[3]) {
    const int h = clampPercent(hunger);
    const int p = clampPercent(happy);
    out[0] = { Stage::YOUNG_MOKO,    Breed::NONE, EVO_BASE_WEIGHT + h * EVO_MOKO_HUNGER_PCT / 100 };
    out[1] = { Stage::YOUNG_SUFFOLK, Breed::NONE, EVO_BASE_WEIGHT + p * EVO_SUFFOLK_HAPPY_PCT / 100 };
    // ワイルド系は「放置気味」（空腹・不幸なほど出やすい）。満腹・幸福の合計が低いほど、重みが増える
    out[2] = { Stage::YOUNG_WILD,    Breed::NONE, EVO_BASE_WEIGHT + (200 - h - p) * EVO_WILD_LOW_PCT / 100 };
}

int Game::adultChoices(Stage young, int hunger, int happy, EvoChoice out[2]) {
    const int h = clampPercent(hunger);
    const int p = clampPercent(happy);
    switch (young) {
        case Stage::YOUNG_MOKO:
            // MERINO は、一旦、抽選に入れない（絵と Breed は残してある）
            out[0] = { Stage::ADULT, Breed::CORRIEDALE, EVO_BASE_WEIGHT };
            out[1] = { Stage::ADULT, Breed::LINCOLN,    EVO_BASE_WEIGHT };
            return 2;
        case Stage::YOUNG_SUFFOLK:
            out[0] = { Stage::ADULT, Breed::SUFFOLK,   EVO_BASE_WEIGHT };
            out[1] = { Stage::ADULT, Breed::HAMPSHIRE, EVO_BASE_WEIGHT + p * EVO_HAMPSHIRE_HAPPY_PCT / 100 };
            return 2;
        case Stage::YOUNG_WILD:
            out[0] = { Stage::ADULT, Breed::MOUFLON, EVO_BASE_WEIGHT };
            out[1] = { Stage::ADULT, Breed::BIGHORN, EVO_BASE_WEIGHT + h * EVO_BIGHORN_HUNGER_PCT / 100 };
            return 2;
        default:
            return 0;
    }
}

int Game::pickWeighted(const EvoChoice* choices, int n, uint32_t roll) {
    int total = 0;
    for (int i = 0; i < n; ++i) total += choices[i].weight;
    if (total <= 0) return 0;
    // 32 ビットの乱数をそのまま使う（127 までしか使わないと、後ろの選択肢が選ばれにくくなる）
    int r = int(roll % uint32_t(total));
    for (int i = 0; i < n; ++i) {
        if (r < choices[i].weight) return i;
        r -= choices[i].weight;
    }
    return n - 1;
}

const char* Game::kindName() const {
    switch (_stage) {
        case Stage::BABY:          return "ベビー";
        case Stage::YOUNG_MOKO:    return "若羊 モコ系";
        case Stage::YOUNG_SUFFOLK: return "若羊 サフォーク系";
        case Stage::YOUNG_WILD:    return "若羊 ワイルド系";
        case Stage::ADULT:
            switch (_breed) {
                case Breed::MERINO:     return "メリノ";
                case Breed::CORRIEDALE: return "コリデール";
                case Breed::LINCOLN:    return "リンカーン";
                case Breed::SUFFOLK:    return "サフォーク";
                case Breed::HAMPSHIRE:  return "ハンプシャー";
                case Breed::MOUFLON:    return "ムフロン";
                case Breed::BIGHORN:    return "ビッグホーン";
                default:                return "ひつじ";
            }
    }
    return "ひつじ";
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
    _sleeping    = false;
    _lifespan_days = uint8_t(LIFESPAN_MIN + (get_rand_32() % LIFESPAN_RANGE));  // 10-15 日
    _dirty      = true;   // 新規開始 / 死亡からの再スタート時は最初の保存を促す
}

void Game::tick() {
    // 墓・命名の間は時間を止める。死ぬと新しいベビーが生まれて墓の画面になり、名前を付けるまで
    // 命名の画面が続く。その間も進めると、名前を付けている間に新しい子が育ったり、進化したり、
    // また死んだりしてしまう。新しい子は、名前を付けてから生きはじめる。
    if (_screen == Screen::GRAVE || _screen == Screen::NAMING) return;

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
        // 毛・角は成体だけ伸ばす（毛刈り・角研ぎができるのは成体だけ）。
        // ベビー・若羊で伸ばすと、成体に進化した時点で毛・角が溜まったままになる。
        // 系統別の手入れ対象だけ伸びる: ワイルド系は角、それ以外（モコ・サフォーク系）は毛。
        if (_stage == Stage::ADULT) {
            constexpr int ADULT_GROW = 2;
            if (family() == Family::WILD) {
                _horn = std::min(100, _horn + ADULT_GROW);
            } else {
                _wool = std::min(100, _wool + ADULT_GROW);
            }
        }

        // 睡眠度の更新は「ゲーム内 1 時間ごと」にゲート。
        // 起動時刻を朝 8 時にオフセットして boot 直後の即就寝を防ぐ。
        if (isNight()) {
            _sleepy = std::min(100, _sleepy + 1);
        } else if (_sleeping) {
            _sleepy = std::max(0, _sleepy - 2);
        }

        _dirty = true;
    }

    if (!_sleeping && _sleepy >= 80) _sleeping = true;
    // 就寝中は操作できないので、遊んでいたミニゲームは中断する（結果は反映しない）
    if (_sleeping && _screen == Screen::MINIGAME) _screen = Screen::MAIN;
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
        if (_action == Action::PET) petCues();
        if (_action == Action::SHEAR) shearCues();
        if (_action == Action::POLISH) polishCues();
        int limit = ACTION_TICKS;
        if (_action == Action::FEED) limit = FEED_ACTION_TICKS;
        if (_action == Action::SHEAR || _action == Action::POLISH) limit = TRIM_ACTION_TICKS;
        if (_walk_tick > limit) _action = Action::NONE;
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
    if (_sleeping && _screen != Screen::MAIN && _screen != Screen::MENU && _screen != Screen::PROFILE) return;

    // LEFT_LONG はバックスペース専用イベント。NAMING の入力中以外は無視
    // （検証用ビルドでは、メイン画面での LEFT_LONG が、姿を次へ進める）。
    if (btn == Button::LEFT_LONG) {
#if defined(DEBUG_FAST) || defined(HOST_TEST)
        if (_screen == Screen::MAIN) debugNextForm();
#endif
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
            break;
        case Screen::MENU:
            if (btn == Button::LEFT) {
                _menu_cursor = (_menu_cursor - 1 + menuCount()) % menuCount();
            } else if (btn == Button::RIGHT) {
                _menu_cursor = (_menu_cursor + 1) % menuCount();
            } else if (btn == Button::CENTER) {
                _screen = Screen::MAIN;              // doAction が画面を変える（ミニゲーム）ことがあるので先に戻す
                doAction(menuAction(_menu_cursor));
            }
            break;
        case Screen::PROFILE:
            _screen = Screen::MAIN;   // 見るだけの画面。どのボタンでも閉じる
            break;
        case Screen::MINIGAME:
            if (_jump.state() == JumpGame::State::OVER) {
                // 終了直後は連打で誤って閉じないよう、OVER_LOCK_MS はボタンを無視する
                if (uint32_t(_now_ms - _jump.overSinceMs()) >= JumpGame::OVER_LOCK_MS) {
                    _screen = Screen::MAIN;
                }
            } else {
                _jump.onPress(_now_ms);
            }
            break;
        case Screen::GRAVE:
            // 墓を確認したら、エンディング曲を止めて、新しい子に名前を付ける
            if (_sound) _sound->stopMelody();
            startNaming();
            break;
        case Screen::NAMING:
            handleNamingButton(btn);
            break;
    }
}

void Game::doAction(Action act) {
    // プロフィールは見るだけなので、就寝中でも開ける
    if (act == Action::PROFILE) {
        _screen = Screen::PROFILE;
        return;
    }

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
            _action = Action::FEED;
            _dirty = true;
            queueSfx(Sfx::MOG);
            break;
        case Action::PET:
            _happy = std::min(100, _happy + 20);
            _action = Action::PET;
            _dirty = true;
            // 音は動きの進行に合わせる：撫でた瞬間に短い音、少しあとに鳴き声（petCues）
            break;
        case Action::SHEAR:
            // モコ系・サフォーク系のみ実効。それ以外は no-op。
            if (_wool > 10) {
                _action_was_grown = isFluffy();
                _wool = 0;
                _happy = std::min(100, _happy + 10);
                _action = Action::SHEAR;
                _dirty = true;
                // 音は動きに合わせて鳴らす（shearCues）
            }
            break;
        case Action::POLISH:
            // ワイルド系のみ実効（角を磨いて短くする）
            if (_horn > 10) {
                _action_was_grown = isLonghorn();
                _horn = 0;
                _happy = std::min(100, _happy + 10);
                _action = Action::POLISH;
                _dirty = true;
                // 音は動きに合わせて鳴らす（polishCues）
            }
            break;
        case Action::MINI:
            // 柵を跳ぶミニゲーム。結果は MISS の時点で 1 回だけ反映する（applyMiniReward）。
            _jump.start(_now_ms, get_rand_32());
            _mini_reward   = 0;
            _screen        = Screen::MINIGAME;
            break;
        default:
            break;
    }
    _walk_tick = 0;
}

// 撫でるの動き（pet_motion）の進行に合わせて音を出す。撫でた瞬間に短い「ポッ」（待たない音）、
// 少し間をおいて「メェ〜」（待つ音なので予約して、描画のあとに鳴らす）。
void Game::petCues() {
    if (pet_motion::isStroke(_walk_tick) && _sound) _sound->blip(1000, 25, _now_ms);
    if (_walk_tick == pet_motion::BLEAT_TICK) queueSfx(Sfx::MEE);
}

// 毛刈りの動き（shear_motion）の進行に合わせて、音を予約する。はさみが閉じるたびに短い「ジョキ」、
// 刈り終えたところで低い「ジョキッ」（待つ音なので、描画のあとに鳴らす）。
void Game::shearCues() {
    if (shear_motion::isSnip(_walk_tick)) queueSfx(Sfx::SNIP);
    else if (_walk_tick == shear_motion::CUT_TICK) queueSfx(Sfx::JOKI);
}

// 角研ぎの動き（polish_motion）の進行に合わせて、幹に押しつけるたびに「ゴシッ」を予約する。
void Game::polishCues() {
    if (polish_motion::isRubStroke(_walk_tick)) queueSfx(Sfx::RUB);
}

void Game::playPendingSfx() {
    Sfx s = _pending_sfx;
    _pending_sfx = Sfx::NONE;
    if (!_sound) return;
    switch (s) {
        case Sfx::MOG:   _sound->mog();   break;
        case Sfx::MEE:   _sound->mee();   break;
        case Sfx::JOKI:  _sound->joki();  break;
        case Sfx::SNIP:  _sound->snip();  break;
        case Sfx::RUB:   _sound->rub();   break;
        case Sfx::HAPPY: _sound->happy(); break;
        case Sfx::ENDING: _sound->playEnding(_now_ms); break;   // 待たずに鳴らし始める（進めるのは main の sound.update）
        case Sfx::NONE:  break;
    }
}

void Game::updateMini() {
    if (_screen != Screen::MINIGAME) return;
    _jump.step(_now_ms);
    uint8_t ev = _jump.consumeEvents();
    if (ev) playMiniSounds(ev);
    if (ev & JumpGame::EV_MISS) applyMiniReward();   // MISS は JumpGame が 1 回だけ出す
}

void Game::playMiniSounds(uint8_t ev) {
    if (!_sound) return;
    // 同じ tick に複数出たら、優先度の高い 1 つだけ鳴らす（ブザーは 1 音しか出せない）。
    if (ev & JumpGame::EV_MISS) {
        _sound->blip(196, 350, _now_ms);
    } else if (ev & JumpGame::EV_CLEARED) {
        // スコア 5 ごとに音程を上げる（最大 6 段）
        _sound->blip(784 + 110 * std::min(_jump.score() / 5, 6), 70, _now_ms);
    } else if (ev & JumpGame::EV_JUMP) {
        _sound->blip(660, 40, _now_ms);
    } else if (ev & JumpGame::EV_COUNT) {
        _sound->blip(440, 80, _now_ms);
    } else if (ev & JumpGame::EV_BEAT) {
        _sound->blip(180, 15, _now_ms);          // 押しどきの合図（小さく低い「コッ」）
    }
}

// スコアに応じて幸福度が上がり、運動でお腹が減る。ミニゲームだけで餓死しないよう空腹は 1 で止める。
// 幸福度は上がる（幸福度は進化の重みに使う値でもある）。
void Game::applyMiniReward() {
    _mini_reward   = std::min(MINI_REWARD_MAX, _jump.score() * MINI_REWARD_PER_SHEEP);
    _happy         = std::min(100, _happy + _mini_reward);
    if (_hunger > 1) _hunger = std::max(1, _hunger - MINI_HUNGER_COST);
    _dirty = true;
}

void Game::evolveYoung() {
    // BABY → 3 系統。進化のときの空腹・幸福の値で重み付けし、残りは乱数（重みは youngChoices を参照）。
    EvoChoice c[3];
    youngChoices(_hunger, _happy, c);
    _stage = c[pickWeighted(c, 3, get_rand_32())].stage;
    _wool = 0;
    _horn = 0;   // 若羊は毛・角を伸ばさない。ベビーの間に溜まった分（古いセーブ）は持ち越さない
    _dirty = true;
    queueSfx(Sfx::HAPPY);
}

void Game::evolveAdult() {
    // 若羊 → 品種。進化のときの空腹・幸福の値で重み付けし、残りは乱数（重みは adultChoices を参照）。
    EvoChoice c[2];
    const int n = adultChoices(_stage, _hunger, _happy, c);
    _breed = c[pickWeighted(c, n, get_rand_32())].breed;

    _stage = Stage::ADULT;
    _wool = 0;
    _horn = 0;           // 若羊の間に溜まった分（古いセーブ）は持ち越さない
    _menu_cursor = 0;    // メニューの項目数が 5 → 6 に変わるので、開いたままでもカーソルの意味がずれないよう先頭へ
    _dirty = true;
    queueSfx(Sfx::HAPPY);
}

#if defined(DEBUG_FAST) || defined(HOST_TEST)
void Game::debugNextForm() {
    // 進化の順。年齢は、その段階が始まる日（ベビー 0 日・若羊 3 日・成体 7 日）にそろえる。
    // ずれていると、次の tick で自然に進化してしまう。日の頭にそろえるので、時刻は朝 8 時のまま（就寝しない）。
    struct Form { Stage stage; Breed breed; uint32_t days; };
    static const Form ORDER[] = {
        { Stage::BABY,          Breed::NONE,       0 },
        { Stage::YOUNG_MOKO,    Breed::NONE,       3 },
        { Stage::ADULT,         Breed::CORRIEDALE, 7 },
        { Stage::ADULT,         Breed::LINCOLN,    7 },
        { Stage::YOUNG_SUFFOLK, Breed::NONE,       3 },
        { Stage::ADULT,         Breed::SUFFOLK,    7 },
        { Stage::ADULT,         Breed::HAMPSHIRE,  7 },
        { Stage::YOUNG_WILD,    Breed::NONE,       3 },
        { Stage::ADULT,         Breed::MOUFLON,    7 },
        { Stage::ADULT,         Breed::BIGHORN,    7 },
    };
    constexpr int N = int(sizeof(ORDER) / sizeof(ORDER[0]));

    // 今の姿の位置。古いセーブの MERINO は、進化先から外れているので、コリデールの位置として扱う
    const Breed cur_breed = (_breed == Breed::MERINO) ? Breed::CORRIEDALE : _breed;
    int cur = 0;
    for (int i = 0; i < N; ++i) {
        if (ORDER[i].stage == _stage && ORDER[i].breed == cur_breed) { cur = i; break; }
    }
    const Form& next = ORDER[(cur + 1) % N];

    _stage       = next.stage;
    _breed       = next.breed;
    _age_ticks   = next.days * TICKS_PER_DAY;
    _lifespan_days = 255;   // 姿を確かめている間に、天寿で死なない
    _hunger      = 100;
    _happy       = 100;
    _wool        = 0;
    _horn        = 0;
    _menu_cursor = 0;       // 若羊 5 項目・成体 6 項目で、カーソルの意味がずれないよう先頭へ
    _dirty       = true;
    queueSfx(Sfx::HAPPY);
}
#endif

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
    queueSfx(Sfx::ENDING);  // 寿命でも餓死でも同じ曲。墓の画面を描いたあとに、main が鳴らし始める
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
