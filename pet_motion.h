#pragma once

// 撫でる（ハートと弾み）。Pico SDK 非依存の純粋な関数で、Display と Game が見て描く・鳴らす。
//
// 撫でるのは 1 回。撫でた瞬間に、羊が 1px 弾んで、ハートが 1 つ頭の真上から浮き上がり、画面の上へ
// 消えていく。少し間をおいて、羊が「メェ〜」と鳴いて応える。動きは、撫でるの動きの経過フレーム t
// （Game::walkTick、0〜Game::ACTION_TICKS）だけで決まる。
namespace pet_motion {

constexpr int STROKE_TICK = 4;      // 撫でるフレーム
constexpr int BOUNCE_LEN  = 4;      // 撫でた瞬間、羊が弾む長さ
constexpr int BOUNCE_PX   = 1;

// 鳴き声（Sound::mee）は、鳴り終わるまで待つ作りで、約 0.37 秒（8 フレーム）画面が止まる。
// 撫でるときの短い音と、続けて 1 つの音に聞こえないよう、0.3 秒（6 フレーム）あけて鳴らす。
// 止まる間もハートが見えているよう、ハートが上がり切る前に鳴らす。
constexpr int BLEAT_TICK        = 10;
constexpr int BLEAT_BLOCK_TICKS = 8;

constexpr int HEART_W = 7, HEART_H = 7;
constexpr int HEART_X0 = 21;             // 羊の左端からの x。羊（48px 幅）の中心（+24）にハートの中心（+3）を合わせる
constexpr int HEART_Y0 = 10;             // ハートが出るときの上端の y（頭のすぐ上）
constexpr int HEART_RISE_FRAMES = 2;     // このフレーム数で 1px 上がる

bool isStroke(int t);                    // 撫でるフレームか

// 羊の弾み。撫でた瞬間は -BOUNCE_PX（上へ）、それ以外は 0。
int  bounce(int t);

// ハート。撫でた瞬間から出て、上へ浮き上がる。
bool heartShown(int t);
int  heartX(int walk_x);                 // 羊の頭の真上（中央揃え）
int  heartY(int t);                      // ハートの上端の y（浮くほど小さくなる。画面の上を出たら負）

// ハートの絵（7x7、左右対称）。dx, dy はハートの左上から。
bool heartPixel(int dx, int dy);

}  // namespace pet_motion
