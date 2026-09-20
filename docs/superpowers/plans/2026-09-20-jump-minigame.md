# ミニゲーム「柵を跳ぶ羊」Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** メニューの「ゲーム」を、ボタンを押すタイミングで柵を跳ぶ、ミスするまで続くリズムゲームにする。

**Architecture:** Pico SDK 非依存の `JumpGame`（固定 5ms グリッドで時刻から進める純粋なロジック）を `Game` が持ち、`Screen::MINIGAME` で操作・描画する。音は `JumpGame` がイベントで知らせ、`Game` が新設の非ブロッキング `Sound::blip` に流す。ミニゲーム中だけ `main.cpp` のループを速くする。

**Tech Stack:** C++17、Pico SDK、ホストテスト（`test/Makefile`、assert マクロの自前方式）。

**Spec:** `docs/superpowers/specs/2026-09-20-jump-minigame-design.md`

## Global Constraints

- コミットメッセージは日本語。末尾に `Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>`
- テストを先に書き、失敗を確認してから実装する（TDD）
- 複雑なロジックにはコメントを追加する
- `JumpGame` は Pico SDK に依存しない（ホストテストでビルドできる）
- 時刻は `uint32_t` の ms。差は必ず符号なしの引き算（`int32_t(a - b)`）で取る
- テストは定数（`JumpGame::JUMP_AIR_MS` など）を参照し、値を直書きしない（窓の概算値をコメントで添えるのは可）
- セーブ形式（`GameSaveData`、MAGIC）は変えない
- `sprites.h` / `kana_font.h` / `ja_font.h` は自動生成物。手編集しない

## 設計上の補足（spec の明確化）

- 「拍」は**ジャンプを押す瞬間**（押しどき）。柵の中心が羊の当たり判定の中心に届くのは、拍の `JUMP_AIR_MS / 2`（250ms）後。ジャンプの頂点で柵の真上を通る。
- 柵が右端（x=128）に現れるのは、当たり判定の中心に届く `LEAD_MS`（1000ms）前 = 拍の 750ms 前。
- 拍の先読みは `PLAN_AHEAD_MS`（1000ms）。柵は現れる 250ms 以上前に計画済みになる。

## ファイル構成

| ファイル | 責務 |
|---|---|
| `jump_game.h` / `jump_game.cpp`（新規） | ミニゲームのルールと時間進行。描画・音・ハードには触れない |
| `game.h` / `game.cpp` | `Screen::MINIGAME`、時刻の受け取り、イベント→音、結果の反映 |
| `sound.h` / `sound.cpp`、`test/stubs/stub_sound.cpp` | 非ブロッキングの `blip` / `update` |
| `display.h` / `display.cpp` | `drawMinigame` |
| `main.cpp` | 時刻ベースのループ、ミニゲーム中の高速ループ、保存の後回し |
| `test/test_game.cpp`、`test/Makefile`、`CMakeLists.txt` | テストとビルド登録 |

---

### Task 1: JumpGame — カウントダウンとジャンプ

**Files:**
- Create: `jump_game.h`、`jump_game.cpp`
- Modify: `test/Makefile`（SOURCES に `../jump_game.cpp`）、`CMakeLists.txt`（`jump_game.cpp`）、`test/test_game.cpp`

**Interfaces:**
- Produces（後続タスクが使う）:
  - 定数（`static constexpr`）: `COUNTDOWN_MS=3000`、`JUMP_AIR_MS=500`、`JUMP_HEIGHT_PX=26`、`FENCE_W=4`、`FENCE_H=12`、`HITBOX_W=12`、`SHEEP_X=16`、`SHEEP_W=24`、`SCROLL_PPS=100`、`SCREEN_W=128`、`GRID_MS=5`、`BPM_START=70`、`BPM_STEP=2`、`BPM_MAX=100`、`MIN_GAP_MARGIN_MS=100`、`OVER_LOCK_MS=800`、`MAX_FENCES=4`、`PLAN_AHEAD_MS=1000`、`FIRST_BEAT_MS=1500`（プレイ開始から最初の拍まで）
  - 導出定数: `HITBOX_CENTER_X = SHEEP_X + SHEEP_W/2`（=28）、`OVERLAP_HALF_MS = (FENCE_W + HITBOX_W) * 1000 / 2 / SCROLL_PPS`（=80）、`LEAD_MS = (SCREEN_W - HITBOX_CENTER_X) * 1000 / SCROLL_PPS`（=1000）
  - `enum class State : uint8_t { IDLE, COUNTDOWN, PLAYING, OVER }`
  - `enum Event : uint8_t { EV_BEAT=1, EV_JUMP=2, EV_CLEARED=4, EV_MISS=8, EV_COUNT=16 }`
  - `void start(uint32_t now_ms, uint32_t seed)`、`void step(uint32_t now_ms)`、`void onPress(uint32_t now_ms)`、`uint8_t consumeEvents()`
  - `State state() const`、`int score() const`、`int countdownNumber() const`（COUNTDOWN 中は 3/2/1、それ以外は 0）、`int sheepHeight() const`、`uint32_t overSinceMs() const`、`uint32_t nowMs() const`

- [ ] **Step 1: 失敗するテストを書く**（`test/test_game.cpp`。`#include "jump_game.h"` を追加し、`int main()` の直前に置く）

```cpp
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
```

`main()` の `RUN` 群に 4 本を追加する。

- [ ] **Step 2: 失敗を確認する**

Run: `cd test && make test 2>&1 | grep error | head -3`
Expected: `jump_game.h` が見つからない、というコンパイルエラー

- [ ] **Step 3: 最小の実装**

`jump_game.h`:

```cpp
#pragma once
#include <cstdint>

// ミニゲーム「柵を跳ぶ羊」のロジック。Pico SDK 非依存。
//
// 時間の扱い：内部を固定 GRID_MS のグリッドで進める。step(now) も onPress(now) も、まず now
// までグリッドを進めてから処理するので、呼び出しの間隔が結果に影響しない。時刻は uint32_t の
// ms で、一周（約 49 日）しても動くよう、差は必ず符号付き 32bit の引き算で取る。
//
// 「拍」= ジャンプを押す瞬間（押しどき）。柵の中心が羊の当たり判定の中心に届くのは、拍の
// JUMP_AIR_MS/2 後。ジャンプの頂点で柵の真上を通る。
class JumpGame {
public:
    enum class State : uint8_t { IDLE, COUNTDOWN, PLAYING, OVER };
    enum Event : uint8_t { EV_BEAT = 1, EV_JUMP = 2, EV_CLEARED = 4, EV_MISS = 8, EV_COUNT = 16 };

    static constexpr uint32_t COUNTDOWN_MS       = 3000;
    static constexpr uint32_t JUMP_AIR_MS        = 500;
    static constexpr int      JUMP_HEIGHT_PX     = 26;
    static constexpr int      FENCE_W            = 4;
    static constexpr int      FENCE_H            = 12;
    static constexpr int      HITBOX_W           = 12;
    static constexpr int      SHEEP_X            = 16;    // 羊スプライトの左端
    static constexpr int      SHEEP_W            = 24;
    static constexpr int      SCREEN_W           = 128;
    static constexpr int      SCROLL_PPS         = 100;   // 柵の流れる速さ（px/秒）
    static constexpr uint32_t GRID_MS            = 5;
    static constexpr int      BPM_START          = 70;
    static constexpr int      BPM_STEP           = 2;
    static constexpr int      BPM_MAX            = 100;
    static constexpr uint32_t MIN_GAP_MARGIN_MS  = 100;   // 1 拍 >= JUMP_AIR_MS + これ
    static constexpr uint32_t OVER_LOCK_MS       = 800;
    static constexpr int      MAX_FENCES         = 4;
    static constexpr uint32_t PLAN_AHEAD_MS      = 1000;  // 拍をこの先まで計画しておく
    static constexpr uint32_t FIRST_BEAT_MS      = 1500;  // プレイ開始から最初の拍まで

    static constexpr int      HITBOX_CENTER_X    = SHEEP_X + SHEEP_W / 2;                    // 28
    static constexpr int      OVERLAP_HALF_MS    = (FENCE_W + HITBOX_W) * 1000 / 2 / SCROLL_PPS;  // 80
    static constexpr int      LEAD_MS            = (SCREEN_W - HITBOX_CENTER_X) * 1000 / SCROLL_PPS;  // 1000

    void start(uint32_t now_ms, uint32_t seed);
    void step(uint32_t now_ms);
    void onPress(uint32_t now_ms);
    uint8_t consumeEvents();

    State    state()          const { return _state; }
    int      score()          const { return _score; }
    int      countdownNumber() const;
    int      sheepHeight()    const { return _height; }
    uint32_t overSinceMs()    const { return _over_since_ms; }
    uint32_t nowMs()          const { return _sim_ms; }

private:
    void advanceTo(uint32_t now_ms);
    void tick(uint32_t t);

    State    _state         = State::IDLE;
    uint32_t _start_ms      = 0;
    uint32_t _sim_ms        = 0;
    uint32_t _play_start_ms = 0;
    uint32_t _over_since_ms = 0;
    uint32_t _jump_start_ms = 0;
    bool     _jumping       = false;
    int      _height        = 0;
    int      _score         = 0;
    int      _last_count    = 3;
    uint8_t  _events        = 0;
};
```

`jump_game.cpp`:

```cpp
#include "jump_game.h"

namespace {
// a - b を符号付きで返す（uint32_t の一周をまたいでも正しい）。
inline int32_t diff(uint32_t a, uint32_t b) { return int32_t(a - b); }
}  // namespace

void JumpGame::start(uint32_t now_ms, uint32_t /*seed*/) {
    _state         = State::COUNTDOWN;
    _start_ms      = now_ms;
    _sim_ms        = now_ms;
    _play_start_ms = now_ms + COUNTDOWN_MS;
    _over_since_ms = 0;
    _jump_start_ms = 0;
    _jumping       = false;
    _height        = 0;
    _score         = 0;
    _last_count    = 3;
    _events        = EV_COUNT;   // 開始時の「3」
}

int JumpGame::countdownNumber() const {
    if (_state != State::COUNTDOWN) return 0;
    int n = 3 - int((_sim_ms - _start_ms) / 1000);
    return n < 1 ? 1 : n;
}

uint8_t JumpGame::consumeEvents() {
    uint8_t e = _events;
    _events = 0;
    return e;
}

void JumpGame::step(uint32_t now_ms) { advanceTo(now_ms); }

void JumpGame::onPress(uint32_t now_ms) {
    advanceTo(now_ms);   // 押下時刻までのグリッドを先に処理してから、その時刻でジャンプを記録する
    if (_state != State::PLAYING || _jumping) return;
    _jumping       = true;
    _jump_start_ms = now_ms;
    _events       |= EV_JUMP;
}

void JumpGame::advanceTo(uint32_t now_ms) {
    if (_state == State::IDLE) return;
    while (diff(now_ms, _sim_ms) >= int32_t(GRID_MS)) {
        _sim_ms += GRID_MS;
        tick(_sim_ms);
    }
}

void JumpGame::tick(uint32_t t) {
    if (_state == State::COUNTDOWN) {
        uint32_t elapsed = t - _start_ms;
        if (elapsed >= COUNTDOWN_MS) {
            _state = State::PLAYING;
            return;
        }
        int n = 3 - int(elapsed / 1000);
        if (n != _last_count) { _last_count = n; _events |= EV_COUNT; }
        return;
    }
    if (_state != State::PLAYING) return;

    // 羊の高さ：放物線。頂点が JUMP_HEIGHT_PX、着地で 0。
    if (_jumping) {
        uint32_t tau = t - _jump_start_ms;
        if (tau >= JUMP_AIR_MS) {
            _jumping = false;
            _height  = 0;
        } else {
            _height = int(4u * JUMP_HEIGHT_PX * tau * (JUMP_AIR_MS - tau) / (JUMP_AIR_MS * JUMP_AIR_MS));
        }
    } else {
        _height = 0;
    }
}
```

`CMakeLists.txt` の `game.cpp` の次の行に `jump_game.cpp` を追加し、`test/Makefile` の `SOURCES` に `../jump_game.cpp` を追加する。

- [ ] **Step 4: 通過を確認する**

Run: `cd test && make test 2>&1 | tail -3`
Expected: `all tests passed`

- [ ] **Step 5: コミット**

```bash
git add jump_game.h jump_game.cpp CMakeLists.txt test/Makefile test/test_game.cpp
git commit -m "JumpGame：カウントダウンとジャンプの土台を追加"
```

---

### Task 2: JumpGame — 拍・柵・衝突・スコア・BPM

**Files:**
- Modify: `jump_game.h`、`jump_game.cpp`、`test/test_game.cpp`

**Interfaces:**
- Consumes: Task 1 の定数・`start` / `step` / `onPress`
- Produces:
  - `struct Fence { uint32_t beat_ms; bool used; bool cleared; }`
  - `const Fence& fence(int i) const`（`0 <= i < MAX_FENCES`）
  - `int fenceCenterX(int i) const`（柵の中心の x。`_sim_ms` 基準）
  - `static int bpmForScore(int score)`、`static uint32_t beatMsForScore(int score)`
  - `start` の `seed` が有効になる（`uint32_t _rng`）

- [ ] **Step 1: 失敗するテストを書く**

```cpp
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
```

`RUN` に 5 本を追加する。

- [ ] **Step 2: 失敗を確認する**

Run: `cd test && make test 2>&1 | grep error | head -3`
Expected: `fence` / `bpmForScore` が未定義、というエラー

- [ ] **Step 3: 実装**

`jump_game.h` の `public:` に追加：

```cpp
    struct Fence { uint32_t beat_ms; bool used; bool cleared; };
    const Fence& fence(int i) const { return _fences[i]; }
    int fenceCenterX(int i) const;   // 描画用。柵の中心の x（_sim_ms 基準）
    static int      bpmForScore(int score);
    static uint32_t beatMsForScore(int score);
```

`private:` に追加：

```cpp
    static constexpr int BEAT_QUEUE = 8;
    bool planBeat();
    uint32_t nextRand();

    Fence    _fences[MAX_FENCES] = {};
    uint32_t _beats[BEAT_QUEUE]  = {};
    int      _beat_head          = 0;
    int      _beat_count         = 0;
    uint32_t _plan_ms            = 0;   // 次に計画する拍の時刻
    int      _gap_left           = 1;   // 次の柵まであと何拍か（1 なら次の拍が柵）
    uint32_t _rng                = 1;
```

`jump_game.cpp`：`start` の `/*seed*/` を有効にし、初期化を足す。

```cpp
void JumpGame::start(uint32_t now_ms, uint32_t seed) {
    // …（Task 1 の初期化に続けて）
    _rng        = seed ? seed : 1u;   // xorshift は 0 だと止まる
    for (Fence& f : _fences) f = Fence{0, false, false};
    _beat_head  = 0;
    _beat_count = 0;
    _plan_ms    = 0;
    _gap_left   = 1;
}

int JumpGame::bpmForScore(int score) {
    int b = BPM_START + BPM_STEP * score;
    return b > BPM_MAX ? BPM_MAX : b;
}

uint32_t JumpGame::beatMsForScore(int score) { return 60000u / uint32_t(bpmForScore(score)); }

uint32_t JumpGame::nextRand() {
    _rng ^= _rng << 13;
    _rng ^= _rng >> 17;
    _rng ^= _rng << 5;
    return _rng;
}

int JumpGame::fenceCenterX(int i) const {
    // 柵の中心は、拍の JUMP_AIR_MS/2 後に羊の当たり判定の中心へ届く。
    uint32_t cross = _fences[i].beat_ms + JUMP_AIR_MS / 2;
    return HITBOX_CENTER_X + int(int64_t(SCROLL_PPS) * diff(cross, _sim_ms) / 1000);
}

// 次の拍を 1 つ計画する。gap（1 または 2 拍）ごとに柵を置く。
// BPM は計画した時点のスコアで決まるので、スコアの反映は PLAN_AHEAD_MS ぶん遅れる。
bool JumpGame::planBeat() {
    if (_beat_count >= BEAT_QUEUE) return false;
    uint32_t t = _plan_ms;
    if (--_gap_left == 0) {
        int slot = -1;
        for (int i = 0; i < MAX_FENCES; ++i) if (!_fences[i].used) { slot = i; break; }
        if (slot < 0) { ++_gap_left; return false; }   // 空きなし（間隔が最短でも 3 本以下なので通常は起きない）
        _fences[slot] = Fence{t, true, false};
        _gap_left = 1 + int(nextRand() & 1u);
    }
    _beats[(_beat_head + _beat_count) % BEAT_QUEUE] = t;
    ++_beat_count;
    _plan_ms = t + beatMsForScore(_score);
    return true;
}
```

`tick()` の COUNTDOWN 終了部を次にし、PLAYING の末尾（高さ計算の後）に柵の処理を足す：

```cpp
        if (elapsed >= COUNTDOWN_MS) {
            _state   = State::PLAYING;
            _plan_ms = _play_start_ms + FIRST_BEAT_MS;
            return;
        }
```

```cpp
    // 拍を先読みで計画する。
    while (diff(_plan_ms, t) < int32_t(PLAN_AHEAD_MS)) {
        if (!planBeat()) break;
    }
    // 到来した拍を通知する。
    while (_beat_count > 0 && diff(t, _beats[_beat_head]) >= 0) {
        _beat_head = (_beat_head + 1) % BEAT_QUEUE;
        --_beat_count;
        _events |= EV_BEAT;
    }
    // 柵：当たり判定と重なる間（|cross - t| < OVERLAP_HALF_MS）に高さが柵に届かなければ衝突。
    // 重なりを抜けたら成功。抜けて十分たったら（画面外へ出たら）枠を空ける。
    for (Fence& f : _fences) {
        if (!f.used) continue;
        int32_t d = diff(f.beat_ms + JUMP_AIR_MS / 2, t);   // 正：まだ手前、負：通り過ぎた
        if (d < OVERLAP_HALF_MS && d > -OVERLAP_HALF_MS) {
            if (_height < FENCE_H) {
                _state = State::OVER;
                _over_since_ms = t;
                _events |= EV_MISS;
                return;
            }
        } else if (d <= -OVERLAP_HALF_MS && !f.cleared) {
            f.cleared = true;
            ++_score;
            _events |= EV_CLEARED;
        }
        if (d <= -int32_t(FENCE_OFFSCREEN_MS)) f.used = false;
    }
```

`public:` の定数に `static constexpr int FENCE_OFFSCREEN_MS = ...;` を追加する（柵の中心が x=-FENCE_W/2 に届くまで）：

```cpp
    // 柵の中心が画面の左端の外（x = -FENCE_W/2）へ出るまでの、当たり判定の中心からの時間
    static constexpr int      FENCE_OFFSCREEN_MS = (HITBOX_CENTER_X + FENCE_W / 2) * 1000 / SCROLL_PPS;   // 300
```

- [ ] **Step 4: 通過を確認する**

Run: `cd test && make test 2>&1 | tail -3`
Expected: `all tests passed`

窓の外側（±150）が失敗にならないなど、意図と違えば、テストではなく `JUMP_HEIGHT_PX` などの定数と計算（`FENCE_H` を超える時間）を確認する。

- [ ] **Step 5: コミット**

```bash
git add jump_game.h jump_game.cpp test/test_game.cpp
git commit -m "JumpGame：拍・柵・衝突判定・スコア・BPM 進行を追加"
```

---

### Task 3: JumpGame — 決定性・刻み非依存・イベント・時刻の一周

**Files:**
- Modify: `test/test_game.cpp`（実装の追加は、テストが失敗した場合だけ）

**Interfaces:**
- Consumes: Task 2 の `jg_autoplay`、`fence(i)`、`consumeEvents()`

- [ ] **Step 1: テストを書く**

```cpp
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
```

`RUN` に 4 本を追加する。

- [ ] **Step 2: 実行する**

Run: `cd test && make test 2>&1 | tail -6`
Expected: 通過する想定。失敗した場合は、その原因（グリッド処理の順序、符号なしの比較など）を `jump_game.cpp` で直す。テストを緩めて通さない。

- [ ] **Step 3: コミット**

```bash
git add test/test_game.cpp jump_game.cpp jump_game.h
git commit -m "JumpGame：決定性・刻み非依存・イベント・時刻の一周のテストを追加"
```

---

### Task 4: Game への組み込み（画面・時刻・結果）

**Files:**
- Modify: `game.h`、`game.cpp`、`test/test_game.cpp`

**Interfaces:**
- Consumes: `JumpGame` 全 API、`Sound::blip`（Task 5。それまでは呼び出しを入れず、Task 5 で足す）
- Produces:
  - `Screen::MINIGAME`（`enum class Screen` に追加）
  - `void Game::setNowMs(uint32_t)`、`void Game::updateMini()`
  - `bool Game::inMiniGame() const`、`const JumpGame& Game::jump() const`、`int Game::miniReward() const`
  - 定数（`game.cpp` の無名 namespace）: `MINI_REWARD_MAX = 30`、`MINI_REWARD_PER_SHEEP = 2`、`MINI_HUNGER_COST = 5`

- [ ] **Step 1: 失敗するテストを書く**

```cpp
// ミニゲームを 1 匹ぶん越えるまで bot で操作するための補助。now を進めながら Game 越しに操作する。
static uint32_t mini_now = 0;

static void mini_step(Game& g, uint32_t ms) {
    for (uint32_t e = 0; e < ms; e += 5) { mini_now += 5; g.setNowMs(mini_now); g.updateMini(); }
}

// clear_target 匹越えたあと押すのをやめ、ゲームオーバーになるまで進める。
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
```

`RUN` に 6 本を追加する。

- [ ] **Step 2: 失敗を確認する**

Run: `cd test && make test 2>&1 | grep error | head -3`
Expected: `setNowMs` / `MINIGAME` などが未定義、というエラー

- [ ] **Step 3: 実装**

`game.h`：`#include "jump_game.h"`、`enum class Screen` に `MINIGAME` を追加。`public:` に：

```cpp
    // ミニゲーム。main.cpp が現在時刻（ms）を渡し、ミニゲーム中は短い周期で updateMini() を呼ぶ。
    void setNowMs(uint32_t now_ms) { _now_ms = now_ms; }
    void updateMini();
    bool inMiniGame() const { return _screen == Screen::MINIGAME; }
    const JumpGame& jump() const { return _jump; }
    int  miniReward() const { return _mini_reward; }   // 直近のゲームで上がった幸福度（表示用）
```

`private:` に：

```cpp
    JumpGame  _jump;
    uint32_t  _now_ms        = 0;
    bool      _mini_rewarded = false;
    int       _mini_reward   = 0;
    void applyMiniReward();
```

`game.cpp`：

- 無名 namespace に `constexpr int MINI_REWARD_MAX = 30; constexpr int MINI_REWARD_PER_SHEEP = 2; constexpr int MINI_HUNGER_COST = 5;`
- `onButton` の MENU 分岐を、画面を先に `MAIN` にしてから `doAction` を呼ぶ順に変える：

```cpp
            } else if (btn == Button::CENTER) {
                _screen = Screen::MAIN;              // doAction が画面を変える（ミニゲーム）ことがあるので先に戻す
                doAction(menuAction(_menu_cursor));
            }
```

- `onButton` の `switch (_screen)` に追加：

```cpp
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
```

- `doAction` の `case Action::MINI:` を置き換える：

```cpp
        case Action::MINI:
            // 柵を跳ぶミニゲーム。結果は MISS の時点で 1 回だけ反映する（applyMiniReward）。
            _jump.start(_now_ms, get_rand_32());
            _mini_rewarded = false;
            _mini_reward   = 0;
            _screen        = Screen::MINIGAME;
            break;
```

- 新規関数：

```cpp
void Game::updateMini() {
    if (_screen != Screen::MINIGAME) return;
    _jump.step(_now_ms);
    uint8_t ev = _jump.consumeEvents();
    if ((ev & JumpGame::EV_MISS) && !_mini_rewarded) applyMiniReward();
}

// スコアに応じて幸福度が上がり、運動でお腹が減る。ミニゲームだけで餓死しないよう空腹は 1 で止める。
// 進化の傾向スコア（tend_*）には反映しない。
void Game::applyMiniReward() {
    _mini_rewarded = true;
    _mini_reward   = std::min(MINI_REWARD_MAX, _jump.score() * MINI_REWARD_PER_SHEEP);
    _happy         = std::min(100, _happy + _mini_reward);
    if (_hunger > 1) _hunger = std::max(1, _hunger - MINI_HUNGER_COST);
    _dirty = true;
}
```

- `tick()`：`if (!_sleeping && _sleepy >= 80) _sleeping = true;` の直後に、就寝したらミニゲームを中断する：

```cpp
    if (_sleeping && _screen == Screen::MINIGAME) _screen = Screen::MAIN;   // 就寝中は操作できないので中断（結果は反映しない）
```

- `display.cpp` の `Display::draw` の `switch` に `case Game::Screen::MINIGAME: break;` を仮置きする（Task 6 で置き換える。未対応 enum の警告を出さないため）。

- [ ] **Step 4: 通過を確認する**

Run: `cd test && make test 2>&1 | tail -3`
Expected: `all tests passed`

- [ ] **Step 5: コミット**

```bash
git add game.h game.cpp display.cpp test/test_game.cpp
git commit -m "Game：ミニゲーム画面と結果（幸福度・空腹）の反映を追加"
```

---

### Task 5: Sound — 非ブロッキングの blip と、ゲームからの効果音

**Files:**
- Modify: `sound.h`、`sound.cpp`、`test/stubs/stub_sound.cpp`、`game.cpp`

**Interfaces:**
- Produces: `void Sound::blip(int freq_hz, int dur_ms, uint32_t now_ms)`、`void Sound::update(uint32_t now_ms)`
- Consumes: `JumpGame::Event`

ホストでは `Sound::blip` の鳴動を確認できないため、テストの追加はなし（スタブは no-op）。`build` / `build_fast` の両ビルドと Task 8 の実機確認で見る。

- [ ] **Step 1: `sound.h` に宣言を追加**

```cpp
    // 非ブロッキングの短い音。鳴らし始めるだけで待たない。止めるのは update() が、
    // now_ms を見て行う（ミニゲーム中は入力を止められないので、mee() などは使わない）。
    void blip(int freq_hz, int dur_ms, uint32_t now_ms);
    void update(uint32_t now_ms);
```

`private:` に `bool _blip_on = false; uint32_t _blip_stop_ms = 0;` と、`void startTone(int freq_hz); void stopTone();` を追加する。

- [ ] **Step 2: `sound.cpp`**：`beep` の PWM 設定を `startTone` / `stopTone` に切り出し、`beep` はそれを使う。

```cpp
void Sound::startTone(int freq_hz) {
    uint32_t sysclk = clock_get_hz(clk_sys);
    uint32_t wrap   = uint32_t(float(sysclk) / kClkDiv / float(freq_hz)) - 1;
    if (wrap > 65535) wrap = 65535;
    pwm_set_wrap(_slice, wrap);
    uint32_t lvl = (uint32_t(_duty) * wrap) / 65535;
    pwm_set_chan_level(_slice, _channel, lvl);
    pwm_set_enabled(_slice, true);
}

void Sound::stopTone() {
    pwm_set_chan_level(_slice, _channel, 0);
    pwm_set_enabled(_slice, false);
}

void Sound::beep(int freq_hz, int dur_ms) {
    if (freq_hz <= 0) { sleep_ms(dur_ms); return; }
    startTone(freq_hz);
    sleep_ms(dur_ms);
    stopTone();
}

void Sound::blip(int freq_hz, int dur_ms, uint32_t now_ms) {
    if (freq_hz <= 0) return;
    startTone(freq_hz);          // 鳴っている最中なら、新しい音に置き換わる
    _blip_on      = true;
    _blip_stop_ms = now_ms + uint32_t(dur_ms);
}

void Sound::update(uint32_t now_ms) {
    if (_blip_on && int32_t(now_ms - _blip_stop_ms) >= 0) {
        stopTone();
        _blip_on = false;
    }
}
```

- [ ] **Step 3: `test/stubs/stub_sound.cpp`** に `void Sound::blip(int, int, uint32_t) {}`、`void Sound::update(uint32_t) {}`、`void Sound::startTone(int) {}`、`void Sound::stopTone() {}` を追加する。コンストラクタの初期化リストは `_pin(pin), _slice(0), _channel(0), _duty(0)` のまま（新メンバーは in-class 初期化）。

- [ ] **Step 4: `game.cpp`**：`updateMini()` で、取り出したイベントを音にする。

```cpp
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
```

`updateMini()` の `consumeEvents()` の直後に `if (ev) playMiniSounds(ev);` を追加し、`game.h` の `private:` に `void playMiniSounds(uint8_t ev);` を宣言する。

`onButton` から `_jump.onPress` した直後のイベント（EV_JUMP）は、次の `updateMini()` で音になる（遅れは最大でループ 1 周ぶん、約 5ms）。

- [ ] **Step 5: 確認とコミット**

Run: `cd test && make test 2>&1 | tail -2 && cd .. && cmake --build build_fast -j 2>&1 | grep -E "error|warning|Built target mokoji"`
Expected: テスト全件通過、`Built target mokoji`

```bash
git add sound.h sound.cpp test/stubs/stub_sound.cpp game.h game.cpp
git commit -m "Sound：非ブロッキングの blip を追加し、ミニゲームの効果音を鳴らす"
```

---

### Task 6: Display — ミニゲームの描画

**Files:**
- Modify: `display.h`、`display.cpp`

**Interfaces:**
- Consumes: `Game::jump()`、`Game::miniReward()`、`JumpGame` の定数と `fence(i)` / `fenceCenterX(i)` / `sheepHeight()` / `countdownNumber()` / `score()` / `state()`、`Display::selectSprite`
- Produces: `void Display::drawMinigame(const Game& g)`

ホストテストの対象外（Pico SDK 依存）。両ビルドの通過と、Task 8 の実機確認で見る。

- [ ] **Step 1: `display.h`** に `void drawMinigame(const Game& g);` を追加する。

- [ ] **Step 2: `display.cpp`**：`draw` の `switch` に `case Game::Screen::MINIGAME: drawMinigame(g); break;` を入れ（Task 4 の仮置きを置き換え）、`drawMinigame` を追加する。

```cpp
// ミニゲーム「柵を跳ぶ羊」。地面 y=52、羊は x=16 に 24x24 で立つ。
void Display::drawMinigame(const Game& g) {
    const JumpGame& j = g.jump();
    constexpr int GROUND_Y = 52;
    char buf[24];

    _oled->fillRect(0, GROUND_Y, SSD1306::W, 1, true);

    // 羊（ジャンプ中は高さぶん持ち上げる）。終了時は地面に立たせたまま。
    auto sprite = selectSprite(g, Game::Face::RIGHT);
    _oled->drawSprite(sprite, JumpGame::SHEEP_X, GROUND_Y - 24 - j.sheepHeight());

    // 柵：中心 x から幅 FENCE_W・高さ FENCE_H の長方形。画面外のものは描かない。
    for (int i = 0; i < JumpGame::MAX_FENCES; ++i) {
        if (!j.fence(i).used) continue;
        int cx = j.fenceCenterX(i);
        if (cx < -JumpGame::FENCE_W || cx > SSD1306::W + JumpGame::FENCE_W) continue;
        _oled->fillRect(cx - JumpGame::FENCE_W / 2, GROUND_Y - JumpGame::FENCE_H,
                        JumpGame::FENCE_W, JumpGame::FENCE_H, true);
    }

    switch (j.state()) {
        case JumpGame::State::COUNTDOWN: {
            std::snprintf(buf, sizeof(buf), "%d", j.countdownNumber());
            constexpr int scale = 3;
            int w = font::textWidth(buf) * scale;
            _oled->drawText(buf, (SSD1306::W - w) / 2, 12, false, scale);
            break;
        }
        case JumpGame::State::PLAYING: {
            std::snprintf(buf, sizeof(buf), "%dひき", j.score());
            _oled->drawText(buf, SSD1306::W - font::textWidth(buf), 0);
            break;
        }
        case JumpGame::State::OVER: {
            std::snprintf(buf, sizeof(buf), "%dひき", j.score());
            constexpr int scale = 2;
            int w = font::textWidth(buf) * scale;
            _oled->drawText(buf, (SSD1306::W - w) / 2, 8, false, scale);
            std::snprintf(buf, sizeof(buf), "しあわせ +%d", g.miniReward());
            w = font::textWidth(buf);
            _oled->drawText(buf, (SSD1306::W - w) / 2, 32);
            break;
        }
        default:
            break;
    }
}
```

- [ ] **Step 3: 確認とコミット**

Run: `cmake --build build_fast -j 2>&1 | grep -E "error|warning|Built target mokoji"`
Expected: `Built target mokoji`

```bash
git add display.h display.cpp
git commit -m "Display：ミニゲームの描画（羊・柵・スコア・カウントダウン・結果）を追加"
```

---

### Task 7: main.cpp — 時刻ベースのループ

**Files:**
- Modify: `main.cpp`

**Interfaces:**
- Consumes: `Game::setNowMs`、`Game::updateMini`、`Game::inMiniGame`、`Sound::update`

- [ ] **Step 1: 定数を追加**（無名 namespace）

```cpp
constexpr uint32_t MINI_LOOP_MS = 5;    // ミニゲーム中のボタン読み取り・更新の周期
constexpr uint32_t MINI_DRAW_MS = 33;   // ミニゲーム中の描画周期（約 30fps。I2C 転送に約 25ms かかる）
```

- [ ] **Step 2: ループを置き換える**

`poll_button` のラムダ内の `uint32_t now_ms = to_ms_since_boot(get_absolute_time());` はそのまま。`while (true)` を次にする。

```cpp
    uint32_t last_tick_ms = to_ms_since_boot(get_absolute_time());
    uint32_t last_draw_ms = 0;

    while (true) {
        const uint32_t now_ms = to_ms_since_boot(get_absolute_time());
        game.setNowMs(now_ms);
        sound.update(now_ms);        // blip の終了時刻を過ぎていたら止める

        // ボタン処理（active LOW、エッジ検出）。ミニゲーム中は約 5ms ごとに読む。
        poll_button(PIN_BTN_LEFT,   bL, Game::Button::LEFT,   true,  Game::Button::LEFT_LONG);
        poll_button(PIN_BTN_CENTER, bC, Game::Button::CENTER, false, Game::Button::CENTER);
        poll_button(PIN_BTN_RIGHT,  bR, Game::Button::RIGHT,  false, Game::Button::RIGHT);
        game.setLeftHeld(bL.pressed);

        // ゲーム全体の tick（年齢・空腹など）は、ループの速さに関係なく経過時間で 50ms ごと。
        while (now_ms - last_tick_ms >= TICK_MS) {
            game.tick();
            last_tick_ms += TICK_MS;
        }
        game.updateMini();

        // 描画。通常は毎ループ、ミニゲーム中は約 33ms ごと。
        const bool mini = game.inMiniGame();
        if (!mini || now_ms - last_draw_ms >= MINI_DRAW_MS) {
            disp.draw(game);
            oled.show();
            last_draw_ms = now_ms;
        }

        // 保存：フラッシュの書き込み中は数十 ms 入力が止まるので、ミニゲーム中は後回しにする。
        int64_t since_save_us = absolute_time_diff_us(last_save, get_absolute_time());
        bool min_elapsed   = since_save_us > int64_t(MIN_SAVE_INTERVAL_MS)   * 1000;
        bool force_elapsed = since_save_us > int64_t(FORCE_SAVE_INTERVAL_MS) * 1000;
        if (!mini && ((game.isDirty() && min_elapsed) || force_elapsed)) {
            save.write(game.saveData());
            game.clearDirty();
            last_save = get_absolute_time();
        }

        sleep_ms(mini ? MINI_LOOP_MS : TICK_MS);
    }
```

- [ ] **Step 3: 両ビルドを確認してコミット**

Run: `cmake --build build -j 2>&1 | grep -E "error|warning|Built target mokoji"; cmake --build build_fast -j 2>&1 | grep -E "error|warning|Built target mokoji"`
Expected: どちらも `Built target mokoji`

```bash
git add main.cpp
git commit -m "main：時刻ベースのループにし、ミニゲーム中は入力・更新を速くして保存を後回しにする"
```

---

### Task 8: 仕上げ（ドキュメント・全体確認・実機・PR）

**Files:**
- Modify: `CLAUDE.md`、`docs/superpowers/specs/2026-09-20-jump-minigame-design.md`（拍の定義の明確化）

- [ ] **Step 1: `CLAUDE.md`** の「アーキテクチャ」のレイヤー構成図に `JumpGame`（ミニゲームのロジック、Pico SDK 非依存）を1行追加し、`Game` 行の説明の下にミニゲームの一文（`Screen::MINIGAME`、`Sound::blip` は非ブロッキング）を追記する。
- [ ] **Step 2: spec** の「ルール」に、拍の定義（押しどき = ジャンプを押す瞬間、柵の中心は拍の 250ms 後に届く）を1文追記する。
- [ ] **Step 3: 全体確認**

Run: `cd test && make clean >/dev/null && make test 2>&1 | tail -2; cd .. && cmake --build build -j 2>&1 | grep -E "error|warning|Built target mokoji"; cmake --build build_fast -j 2>&1 | grep -E "error|warning|Built target mokoji"`
Expected: テスト全件通過、両ビルドとも `Built target mokoji`

- [ ] **Step 4: 実機**：`picotool load -f -x build_fast/mokoji.uf2` で書き込み、次を確認する（ユーザーが操作）。
  - メニューの「ゲーム」→ カウントダウン 3・2・1 → 柵が流れ、拍の「コッ」に合わせて押すと跳ぶ
  - 押しどきの合図が分かりやすいか、押してから跳ぶまでの遅れが気にならないか
  - 音でループが止まらない（連打しても引っかからない）
  - ミス → 転ぶ画面 → 「N ひき」と「しあわせ +M」→ ボタンでメインへ
  - 幸福度・空腹が反映される
- [ ] **Step 5: コミットして PR**（`/pr`）

```bash
git add CLAUDE.md docs/superpowers/specs/2026-09-20-jump-minigame-design.md
git commit -m "ミニゲームの構成を CLAUDE.md に追記し、設計書の拍の定義を明確化"
```

---

## Self-Review

**1. Spec coverage**
- ルール（ジャンプ・二段なし・衝突・拍・BPM・カウントダウン・終了ロック）: Task 1–2、Task 4（ロック）
- 時間の扱い（5ms グリッド・一周・先読み・最初の押しどき・乱数）: Task 1–3
- クラス構成・`Game` の変更（画面・`setNowMs`・`updateMini`・メニュー確定の順序・就寝中の拒否・別画面への破棄）: Task 4（就寝中の拒否は既存の `doAction` 先頭。テストは既存の `test_sleeping_other_actions_are_rejected` が `Action::MINI` を含む）
- 結果（幸福度・空腹・下限・`tend_*`・1 回だけ・dirty）: Task 4
- 音・スタブ: Task 5
- 画面: Task 6
- メインループ（速いループ・tick の時刻ベース・`Sound::update`・保存の後回し）: Task 7
- テスト方針・エッジケース（一周）: Task 1–4
- 対象外（最高記録・傾向スコア）: 実装しない

**2. Placeholder scan**: TBD・「後で」なし。Task 8 のドキュメント追記は、追記する内容を明記した。

**3. Type consistency**: `Fence{beat_ms, used, cleared}`、`fence(i)`、`fenceCenterX(i)`、`consumeEvents()`、`EV_*`、`State::*`、`setNowMs` / `updateMini` / `inMiniGame` / `jump()` / `miniReward()`、`blip(int,int,uint32_t)` / `update(uint32_t)` を、全タスクで揃えた。

**既知の注意（実装時に確認）**
- ゲーム全体の `tick()` が、ループ回数ではなく経過時間（50ms）ごとになる。今のループは `sleep_ms(50)` に描画の約 25ms が加わり約 75ms で 1 周しているため、通常時のゲームの進みが約 1.5 倍になる（設計書の「1 game-hour = 実時間 1 時間」には合う）。この変更は最後にユーザーへ報告する。元の周期を保ちたければ、`TICK_MS` を約 75ms にする 1 行で戻せる。
