#pragma once

// 背景（草の地面・夜の空）。Pico SDK 非依存の純粋な関数で、Display が見てピクセルを打つ。
//
// 背景は羊の後ろではなく、羊と重ならない上下の帯に置く。羊のスプライトは点いているピクセルだけを
// 描くので、背後に背景があると、目や穴の部分から透けて汚く見えるため。
//   空（月・星）: y = 0〜SKY_BOTTOM    羊の上端（y >= 20）より上
//   地面（草）  : y = GRASS_TOP〜H-1   羊の足元（y <= 53）より下
namespace background {

constexpr int W          = 128;
constexpr int H          = 64;
constexpr int GRASS_TOP  = 56;
constexpr int SKY_BOTTOM = 17;

constexpr int MOON_X = 110, MOON_Y = 3;
constexpr int MOON_W = 8,   MOON_H = 11;

constexpr int STAR_COUNT = 10;
extern const int STARS[STAR_COUNT][2];   // 星の中心 (x, y)

// 草の地面：一番下の行は点線、その上に不揃いな房（中央が高く、左右が低く、先が外へ傾く）。
// 決まった疑似乱数で並べるので、毎回同じ絵になる。
bool grassPixel(int x, int y);

// 右上の三日月（")" 型）。
bool moonPixel(int x, int y);

// 昼の空。太陽は左上に固定する（右上の月と左右で対になる）。点滅はしない。
// 静止した明るい塊が何時間も光り続けると OLED が焼き付くので、Display は他の背景と同じ
// ゆっくりした揺れ（±2px）を足して描く。
constexpr int SUN_W = 9, SUN_H = 9;
constexpr int SUN_X = 12, SUN_Y = 3;
bool sunPixel(int x, int y);

// 雲：2 つがゆっくり右へ流れ、右端を出たら左端から戻る。tick は Game::ageTicks（約 50ms ごと）。
bool cloudPixel(int x, int y, int tick);

// 星（1px の点）。3 つに 1 つは十字（＋）で、frame（0/1 など）が変わると十字になる星が入れ替わる。
// 星の中心は、どのフレームでも点いている。
bool starPixel(int x, int y, int frame);

}  // namespace background
