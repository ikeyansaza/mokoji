#include "sound.h"
#include "hardware/pwm.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"
#include "pico/time.h"
#include "pico/rand.h"

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
    if (freq_hz <= 0) {
        sleep_ms(dur_ms);
        return;
    }
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
    // 一周（約 49 日）をまたいでも比較できるよう、符号付きの差で見る
    if (_blip_on && int32_t(now_ms - _blip_stop_ms) >= 0) {
        stopTone();
        _blip_on = false;
    }

    // エンディング曲: 鳴らす音が変わったときだけ、ブザーを切り替える（休符・音の切れ目は無音）
    if (_melody.playing() && _melody.update(now_ms)) {
        const int f = _melody.freq();
        if (f > 0) startTone(f); else stopTone();
    }
}

void Sound::playEnding(uint32_t now_ms) {
    stopTone();
    _blip_on = false;
    _melody.start(melody::kEnding, melody::kEndingCount, now_ms);
    update(now_ms);   // 最初の音をすぐ鳴らす
}

void Sound::stopMelody() {
    if (!_melody.playing()) return;
    _melody.stop();
    stopTone();
}

void Sound::glide(int from_hz, int to_hz, int dur_ms) {
    constexpr int STEP_MS = 4;
    for (int t = 0; t < dur_ms; t += STEP_MS) {
        startTone(from_hz + (to_hz - from_hz) * t / dur_ms);
        sleep_ms(STEP_MS);
    }
}

void Sound::warble(int center_hz, int depth_hz, int rate_hz, int dur_ms) {
    constexpr int STEP_MS = 4;
    for (int t = 0; t < dur_ms; t += STEP_MS) {
        // 三角波で -1000..+1000 を往復する（周期 = 1000 / rate_hz ms）
        int q   = (t * rate_hz) % 1000;
        int tri = (q < 500) ? (4 * q - 1000) : (3000 - 4 * q);
        startTone(center_hz + depth_hz * tri / 1000);
        sleep_ms(STEP_MS);
    }
}

void Sound::mee() {
    // 「メェ〜」：羊の鳴き声らしさは、音の細かい揺れ（震え）。
    // 以前の 440→494→440Hz は、低くてブザーの濁った音になる上に、最後に元へ戻ってしぼむ形が
    // 失敗音に聞こえた。ここでは 600Hz 前後で、震わせて、最後は持ち上げて終わる（かわいがられて嬉しい感じ）。
    // なお、速く深い震え（毎秒 24 回・±70Hz）とざらつき（途切れ）を付けたら、実機で聞いて悪くなったので
    // 穏やかな震え（毎秒 9 回・±45Hz）に戻した。
    glide(520, 640, 70);        // 「メ」：低めから立ち上がる
    warble(640, 45, 9, 240);    // 「ェ〜」：震わせながら伸ばす
    glide(640, 720, 60);        // 最後は少し持ち上げる
    stopTone();
}

void Sound::mog() {
    // 「もぐもぐ」：交互に跳ねるリズムで、最後に一番高い音で終わる。
    // 以前の 300→350→300Hz は、低くてブザーの濁った音になる上に、下がって終わる形が
    // 「失敗・エラー」に聞こえた。ここでは上がって終わる形（happy() と同じ向き）にする。
    const int freqs[] = {523, 659, 523, 659, 784};
    for (int f : freqs) {
        beep(f, 40);
        sleep_ms(20);
    }
}

// パッシブブザーは単音の矩形波しか出せないので、周波数を 2ms ごとにランダムに飛ばして
// ノイズのようなザラザラした音を作る。ブザーは 2kHz 以上で共鳴して耳障りになるので、1kHz 前後に抑える。
void Sound::noise(int dur_ms, int lo_hz, int hi_hz) {
    for (int t = 0; t < dur_ms; t += 2) {
        startTone(lo_hz + int(get_rand_32() % uint32_t(hi_hz - lo_hz)));
        sleep_ms(2);
    }
    stopTone();
}

void Sound::joki() {
    // 「ジョキ、ジョキ、ジョキッ」：刃が閉じる音（ノイズの短いバースト）を間を空けて繰り返す。
    // 最後だけ少し長く、低めの帯域にして「ジョキッ」と刈り切った感じにする。
    noise(60, 1000, 1900);
    sleep_ms(90);
    noise(60, 900, 1800);
    sleep_ms(90);
    noise(60, 1000, 1900);
    sleep_ms(90);
    noise(110, 600, 1400);
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
