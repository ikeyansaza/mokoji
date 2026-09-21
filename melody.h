#pragma once
#include <cstdint>

// 音符列を、時刻から進める純粋なロジック（Pico SDK に依存しない。ブザーは Sound が鳴らす）。
// ミニゲームの blip と同じく待たずに進める: 呼ぶたびに今の時刻に合った音を返すので、
// ループが遅れて何音分もまとめて進んでも、曲の長さはずれない。
namespace melody {

struct Note {
    uint16_t freq_hz;   // 0 = 休符
    uint16_t dur_ms;
};

class MelodyPlayer {
public:
    // 音の切れ目（各音の終わりの無音）。同じ高さの音が続いても、別々の音に聞こえるようにする。
    // 短い音（切れ目の 3 倍未満）には付けない。
    static constexpr uint32_t GAP_MS = 30;

    void start(const Note* notes, int count, uint32_t now_ms);
    void stop();
    bool playing() const { return _playing; }
    int  freq() const { return _out; }   // 今鳴らす周波数（0 = 無音）

    // now_ms までの進行を反映する。鳴らす周波数が変わったら true（呼び出し側は、その音に切り替える）。
    bool update(uint32_t now_ms);

private:
    const Note* _notes = nullptr;
    int         _count = 0;
    int         _index = 0;
    uint32_t    _note_start_ms = 0;   // 今の音が始まった時刻
    bool        _playing = false;
    int         _out = 0;
};

// エンディング曲（亡くなったときの画面で流す。オリジナル、約 29 秒、ハ長調）。
extern const Note kEnding[];
extern const int  kEndingCount;

}  // namespace melody
