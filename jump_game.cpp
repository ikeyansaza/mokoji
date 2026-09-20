#include "jump_game.h"

namespace {
// a - b を符号付きで返す（uint32_t の一周をまたいでも正しい）。
inline int32_t diff(uint32_t a, uint32_t b) { return int32_t(a - b); }
}  // namespace

void JumpGame::start(uint32_t now_ms, uint32_t seed) {
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
    _rng           = seed ? seed : 1u;   // xorshift は 0 だと止まる
    for (Fence& f : _fences) f = Fence{0, false, false};
    _beat_head     = 0;
    _beat_count    = 0;
    _plan_ms       = 0;
    _gap_left      = 1;
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

void JumpGame::tick(uint32_t t) {
    if (_state == State::COUNTDOWN) {
        uint32_t elapsed = t - _start_ms;
        if (elapsed >= COUNTDOWN_MS) {
            _state   = State::PLAYING;
            _plan_ms = _play_start_ms + FIRST_BEAT_MS;
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
        if (d <= -FENCE_OFFSCREEN_MS) f.used = false;
    }
}
