#pragma once

// 角研ぎ（木の幹に角をこすりつけて、火花が散る）。Pico SDK 非依存の純粋な関数で、Display が見て描く。
//
// 動きは、角研ぎの動きの経過フレーム t（Game::walkTick、0〜Game::TRIM_ACTION_TICKS）だけで決まる。
// 羊の横に木の幹が立つ（最初から）。こする間（RUB_START〜RUB_END）は、羊が幹のほうへ体を押しつけては
// 戻すのを繰り返し、押しつけるたびに、角の先に火花が散る。こすり終わったところ（FILED_TICK）で、
// 羊の絵が、角の長い絵から通常の絵に切り替わる。
namespace polish_motion {

// 角研ぎの音は、毛刈りと同じ Sound::joki（暫定）で、約 0.56 秒（11 フレーム）のあいだ画面が止まる。
// 止まっている間に時間が過ぎるので、こすり始めは、音が終わってから。
constexpr int SOUND_BLOCK_TICKS = 12;

constexpr int RUB_START  = 14;
constexpr int RUB_END    = 38;      // ここでこすり終わる
constexpr int FILED_TICK = RUB_END; // 角が短くなる（絵が切り替わる）

constexpr int LEAN_PX = 4;          // 幹へ押しつける量。戻すときは、LEAN_PX - 2

constexpr int TRUNK_W = 8;
constexpr int TRUNK_H = 34;
constexpr int TRUNK_TOP = 22;       // 幹の上端の y。根元（最下行）は 55 で、草の帯（y=56〜）の真上
constexpr int SPARK_W = 5, SPARK_H = 5;
constexpr int SPARK_CENTER_Y = 30;  // 火花の中心の y（角の高さ）

// 幹を置く向き。羊が右半分（walk_x > 56）にいるときは左に置く（画面から、はみ出さないため）。
int  direction(int walk_x);              // +1：右に幹  /  -1：左に幹
int  trunkLeft(int walk_x);              // 幹の左端の x。羊の 2 倍スプライト（48px 幅）の外に置く

bool isRubbing(int t);                   // こする間か
bool isPressed(int t);                   // 幹へ押しつけている瞬間か（こする間の 2 フレームおき）
bool filed(int t);                       // 角を研ぎ終えたか（羊の絵を通常に切り替える）

// 幹の向き dir へ体を傾ける量。こする間だけ、押しつける・戻すを繰り返す。それ以外は 0。
void lean(int t, int dir, int* dx, int* dy);

// 火花。押しつけている間だけ出る。x, y は左上の座標。
bool sparkShown(int t);
void sparkPos(int walk_x, int* x, int* y);

// 幹の絵（8x34）と、火花の絵（5x5）。dx, dy は左上から。
bool trunkPixel(int dx, int dy);
bool sparkPixel(int dx, int dy);

}  // namespace polish_motion
