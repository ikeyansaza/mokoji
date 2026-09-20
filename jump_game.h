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

    static constexpr int      HITBOX_CENTER_X    = SHEEP_X + SHEEP_W / 2;                            // 28
    static constexpr int      OVERLAP_HALF_MS    = (FENCE_W + HITBOX_W) * 1000 / 2 / SCROLL_PPS;     // 80
    static constexpr int      LEAD_MS            = (SCREEN_W - HITBOX_CENTER_X) * 1000 / SCROLL_PPS; // 1000
    // 柵の中心が画面の左端の外（x = -FENCE_W/2）へ出るまでの、当たり判定の中心からの時間
    static constexpr int      FENCE_OFFSCREEN_MS = (HITBOX_CENTER_X + FENCE_W / 2) * 1000 / SCROLL_PPS;   // 300

    struct Fence { uint32_t beat_ms; bool used; bool cleared; };

    void start(uint32_t now_ms, uint32_t seed);
    void step(uint32_t now_ms);
    void onPress(uint32_t now_ms);
    uint8_t consumeEvents();

    State    state()           const { return _state; }
    int      score()           const { return _score; }
    int      countdownNumber() const;
    int      sheepHeight()     const { return _height; }
    uint32_t overSinceMs()     const { return _over_since_ms; }
    uint32_t nowMs()           const { return _sim_ms; }
    const Fence& fence(int i)  const { return _fences[i]; }
    int      fenceCenterX(int i) const;   // 描画用。柵の中心の x（_sim_ms 基準）

    static int      bpmForScore(int score);
    static uint32_t beatMsForScore(int score);

private:
    static constexpr int BEAT_QUEUE = 8;

    void     advanceTo(uint32_t now_ms);
    void     tick(uint32_t t);
    bool     planBeat();
    uint32_t nextRand();

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

    Fence    _fences[MAX_FENCES] = {};
    uint32_t _beats[BEAT_QUEUE]  = {};
    int      _beat_head          = 0;
    int      _beat_count         = 0;
    uint32_t _plan_ms            = 0;   // 次に計画する拍の時刻
    int      _gap_left           = 1;   // 次の柵まであと何拍か（1 なら次の拍が柵）
    uint32_t _rng                = 1;
};
