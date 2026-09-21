#pragma once
#include <cstdint>
#include "pico/types.h"

// パッシブブザー効果音（PWM 制御）。MicroPython 版の Sound クラス相当。
class Sound {
public:
    explicit Sound(uint pin);

    void mee();    // メェ〜（羊の鳴き声。震わせて伸ばし、最後に持ち上げる）
    void mog();    // もぐもぐ（餌やり）
    void joki();   // ジョキジョキ（毛刈り）
    void happy();  // 喜び（イベント）

    void setVolume(int vol_0_to_10);

    // 非ブロッキングの短い音。鳴らし始めるだけで待たない。止めるのは update() が、now_ms を
    // 見て行う（ミニゲーム中は入力を止められないので、待つ mee() などは使わない）。
    void blip(int freq_hz, int dur_ms, uint32_t now_ms);
    void update(uint32_t now_ms);

private:
    uint _pin;
    uint _slice;
    uint _channel;
    uint16_t _duty;

    bool     _blip_on      = false;
    uint32_t _blip_stop_ms = 0;

    void noise(int dur_ms, int lo_hz, int hi_hz);   // 周波数をランダムに飛ばしたザラザラ音（待つ）
    // 音を止めずに周波数を動かす（待つ）。続けて呼ぶと 1 つの音として続く。止めるのは呼び出し側の stopTone()。
    void glide(int from_hz, int to_hz, int dur_ms);                  // なめらかに from → to
    void warble(int center_hz, int depth_hz, int rate_hz, int dur_ms);  // center を中心に ±depth で rate_hz で揺らす
    void startTone(int freq_hz);   // PWM を周波数 freq_hz で鳴らし始める（待たない）
    void stopTone();
    void beep(int freq_hz, int dur_ms);
};
