#include "melody.h"

namespace melody {

void MelodyPlayer::start(const Note* notes, int count, uint32_t now_ms) {
    _notes         = notes;
    _count         = count;
    _index         = 0;
    _note_start_ms = now_ms;
    _playing       = (notes != nullptr && count > 0);
    _out           = 0;   // 最初の update で、先頭の音への切り替えが「変化」として返る
}

void MelodyPlayer::stop() {
    _playing = false;
    _out     = 0;
}

bool MelodyPlayer::update(uint32_t now_ms) {
    if (!_playing) return false;

    // 一周（約 49 日）をまたいでも比べられるよう、符号付きの差で見る。
    // 遅れて呼ばれても、終わった音を飛ばして、今の時刻の音まで進める（累積で進めるのでずれが溜まらない）。
    int32_t elapsed = int32_t(now_ms - _note_start_ms);
    while (_playing && elapsed >= int32_t(_notes[_index].dur_ms)) {
        elapsed        -= int32_t(_notes[_index].dur_ms);
        _note_start_ms += _notes[_index].dur_ms;
        if (++_index >= _count) _playing = false;
    }

    int out = 0;
    if (_playing) {
        const Note& n = _notes[_index];
        const bool in_gap = n.dur_ms >= 3 * GAP_MS && elapsed >= int32_t(n.dur_ms - GAP_MS);
        out = in_gap ? 0 : n.freq_hz;
    }
    const bool changed = (out != _out);
    _out = out;
    return changed;
}

// エンディング曲。ハ長調、8 分音符 = 360 ms、4/4 拍子で 10 小節（80 拍 × 360 ms = 28.8 秒）。
// 4 小節ずつ「問い → 呼びかけ」を 2 回（1〜4、5〜8）くり返し、最後に最初のフレーズを戻して、ド（主音）を長く伸ばして終わる。
// 音は 523〜1047 Hz（ド〜高いド）。パッシブブザーで、音程がはっきり聞こえて、うるさくない音域。
// 実機で聞いて、テンポ（E）や音を直す。音は並びを変えるだけで、他は触らなくてよい。
namespace {
constexpr uint16_t E = 360;   // 8 分音符の長さ（ms）
constexpr uint16_t C5 = 523, D5 = 587, E5 = 659, F5 = 698, G5 = 784, A5 = 880, B5 = 988, C6 = 1047;
constexpr uint16_t R = 0;     // 休符
}

const Note kEnding[] = {
    // 1〜2 小節: そっと始まる最初のフレーズ（ミ・ソ・ラ → ミ・レ・ド）
    { E5, 2 * E }, { G5, 2 * E }, { A5, 3 * E }, { G5, 1 * E },
    { E5, 2 * E }, { D5, 2 * E }, { C5, 4 * E },
    // 3〜4 小節: 少し上がって、レで止まる（まだ終わらない感じ）
    { D5, 2 * E }, { E5, 2 * E }, { G5, 3 * E }, { E5, 1 * E },
    { D5, 6 * E }, { R, 2 * E },
    // 5〜6 小節: いちばん高いところ（ラ・高いド・シ）から降りてくる
    { A5, 2 * E }, { C6, 2 * E }, { B5, 3 * E }, { A5, 1 * E },
    { G5, 2 * E }, { A5, 2 * E }, { E5, 4 * E },
    // 7〜8 小節: ゆっくり下がって、またレで息をつく
    { F5, 2 * E }, { A5, 2 * E }, { G5, 3 * E }, { E5, 1 * E },
    { D5, 2 * E }, { E5, 2 * E }, { D5, 4 * E },
    // 9〜10 小節: 最初のフレーズを短く戻して、ドを長く伸ばして終わる
    { E5, 2 * E }, { G5, 2 * E }, { E5, 2 * E }, { D5, 2 * E },
    { C5, 8 * E },
};
const int kEndingCount = int(sizeof(kEnding) / sizeof(kEnding[0]));

}  // namespace melody
