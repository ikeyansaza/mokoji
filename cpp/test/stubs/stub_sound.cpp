// Host-test 用の Sound 実装。本物 (cpp/sound.cpp) は hardware/pwm.h 等の Pico SDK に
// 依存して macOS では link できないので、no-op 実装を別途用意してテストにリンクする。
#include "sound.h"

Sound::Sound(uint pin) : _pin(pin), _slice(0), _channel(0), _duty(0) {}

void Sound::mee()    {}
void Sound::mog()    {}
void Sound::joki()   {}
void Sound::happy()  {}
void Sound::setVolume(int) {}
void Sound::beep(int, int) {}
