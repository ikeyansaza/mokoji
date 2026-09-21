// Host-test 用の Sound 実装。本物 (cpp/sound.cpp) は hardware/pwm.h 等の Pico SDK に
// 依存して macOS では link できないので、no-op 実装を別途用意してテストにリンクする。
#include "sound.h"

// テストから、エンディング曲の開始・停止が呼ばれた回数を確かめる。
int g_stub_ending_started  = 0;
int g_stub_melody_stopped  = 0;

Sound::Sound(uint pin) : _pin(pin), _slice(0), _channel(0), _duty(0) {}

void Sound::mee()    {}
void Sound::mog()    {}
void Sound::joki()   {}
void Sound::snip()   {}
void Sound::rub()    {}
void Sound::happy()  {}
void Sound::setVolume(int) {}
void Sound::beep(int, int) {}
void Sound::startTone(int) {}
void Sound::stopTone() {}
int g_stub_blip_count = 0;   // Sound::blip が呼ばれた回数（テストが数える）
void Sound::blip(int, int, uint32_t) { ++g_stub_blip_count; }
void Sound::update(uint32_t) {}
void Sound::playEnding(uint32_t) { ++g_stub_ending_started; }
void Sound::stopMelody()         { ++g_stub_melody_stopped; }
void Sound::noise(int, int, int) {}
void Sound::glide(int, int, int) {}
void Sound::warble(int, int, int, int) {}
