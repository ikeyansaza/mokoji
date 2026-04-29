#pragma once
#include <cstdint>
#include "pico/types.h"

// パッシブブザー効果音（PWM 制御）。MicroPython 版の Sound クラス相当。
class Sound {
public:
    explicit Sound(uint pin);

    void mee();    // メェ〜（なでる）
    void mog();    // もぐもぐ（餌やり）
    void joki();   // ジョキジョキ（毛刈り）
    void happy();  // 喜び（イベント）

    void setVolume(int vol_0_to_10);

private:
    uint _pin;
    uint _slice;
    uint _channel;
    uint16_t _duty;

    void beep(int freq_hz, int dur_ms);
};
