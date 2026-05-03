#include "sound.h"
#include "hardware/pwm.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"
#include "pico/time.h"

namespace {
// PWM 分周をある程度大きく取り、低中音域 (~100Hz–2kHz) を全部 wrap 65535 以下で扱えるようにする。
constexpr float kClkDiv = 64.0f;
}

Sound::Sound(uint pin) : _pin(pin), _duty(2000) {
    gpio_set_function(_pin, GPIO_FUNC_PWM);
    _slice   = pwm_gpio_to_slice_num(_pin);
    _channel = pwm_gpio_to_channel(_pin);
    pwm_set_clkdiv(_slice, kClkDiv);
    pwm_set_chan_level(_slice, _channel, 0);
    pwm_set_enabled(_slice, false);
}

void Sound::beep(int freq_hz, int dur_ms) {
    if (freq_hz <= 0) {
        sleep_ms(dur_ms);
        return;
    }
    uint32_t sysclk = clock_get_hz(clk_sys);
    uint32_t wrap   = uint32_t(float(sysclk) / kClkDiv / float(freq_hz)) - 1;
    if (wrap > 65535) wrap = 65535;

    pwm_set_wrap(_slice, wrap);
    uint32_t lvl = (uint32_t(_duty) * wrap) / 65535;
    pwm_set_chan_level(_slice, _channel, lvl);
    pwm_set_enabled(_slice, true);
    sleep_ms(dur_ms);
    pwm_set_chan_level(_slice, _channel, 0);
    pwm_set_enabled(_slice, false);
}

void Sound::mee() {
    beep(440, 80);
    sleep_ms(20);
    beep(494, 120);
    sleep_ms(20);
    beep(440, 200);
}

void Sound::mog() {
    // 300→350→300 で「もぐもぐ」感を出す音階変化
    beep(300, 50);
    sleep_ms(30);
    beep(350, 50);
    sleep_ms(30);
    beep(300, 50);
}

void Sound::joki() {
    const int freqs[] = {600, 500, 600, 500};
    for (int f : freqs) {
        beep(f, 60);
        sleep_ms(20);
    }
}

void Sound::happy() {
    const int freqs[] = {523, 659, 784};
    for (int f : freqs) {
        beep(f, 100);
        sleep_ms(30);
    }
}

void Sound::setVolume(int vol_0_to_10) {
    if (vol_0_to_10 < 0)  vol_0_to_10 = 0;
    if (vol_0_to_10 > 10) vol_0_to_10 = 10;
    _duty = uint16_t(vol_0_to_10 * 6553);  // MicroPython 版と揃える
}
