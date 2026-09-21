#pragma once

// 毛刈り（はさみが体を 2 往復して、毛の束が落ちていく）。Pico SDK 非依存の純粋な関数で、Display が見て描く。
//
// 動きは、毛刈りの動きの経過フレーム t（Game::walkTick、0〜Game::TRIM_ACTION_TICKS）だけで決まる。
// はさみは、羊の体の上を 1 回目は右へ、2 回目は少し下を左へ動く。動きながら毛の束が落ちて、地面にたまる。
// 2 回目が終わったところ（CUT_TICK）で、羊の絵が、毛のふさふさな絵から通常の絵に切り替わる。
namespace shear_motion {

// 毛刈りの音（Sound::joki）は、鳴り終わるまで待つ作りで、約 0.56 秒（11 フレーム）のあいだ画面が止まる。
// 止まっている間に時間が過ぎるので、はさみは、音が終わってから出す。
constexpr int SOUND_BLOCK_TICKS = 12;

constexpr int PASS_COUNT    = 2;
constexpr int PASS_START[PASS_COUNT] = { 14, 28 };   // はさみが動き始めるフレーム
constexpr int PASS_LEN      = 14;                    // 1 回の長さ
constexpr int CUT_TICK      = PASS_START[1] + PASS_LEN;   // 2 回目が終わる。毛が消えて、はさみも消える

constexpr int SCISSORS_W = 7, SCISSORS_H = 9;
constexpr int PASS_Y[PASS_COUNT] = { 24, 34 };       // はさみの上端の y（1 回目は頭〜肩、2 回目は胴）
constexpr int PASS_X0 = 8;                           // 羊の左端からの x（はさみの左端）。ここから
constexpr int PASS_X1 = 34;                          // ここまで動く（羊の絵の幅 48px の中に収まる）

constexpr int TUFT_COUNT  = 6;
constexpr int TUFT_SPAWN[TUFT_COUNT] = { 16, 20, 24, 30, 34, 38 };   // 毛の束が落ち始めるフレーム
constexpr int TUFT_W = 5, TUFT_H = 4;
constexpr int TUFT_FALL_PX = 2;                      // 1 フレームに落ちる量
constexpr int GROUND_Y     = 55;                     // 毛の束が積もる地面の最下行（草の帯 y=56〜 の真上）

bool clipperShown(int t);                            // はさみが見えているか
int  clipperX(int walk_x, int t);                    // はさみの左端の x
int  clipperY(int t);                                // はさみの上端の y
bool clipperOpen(int t);                             // 刃が開いているか（2 フレームごとに開閉）
bool woolCut(int t);                                 // 毛が刈り取られたか（羊の絵を通常に切り替える）

// 毛の束 i。落ちている・積もっているあいだ true を返し、左上の座標を x, y に書く。まだ落ち始めていなければ false。
bool tuft(int i, int walk_x, int t, int* x, int* y);

// はさみの絵（7x9）と、毛の束の絵（5x4）。dx, dy は左上から。
bool scissorsPixel(bool open, int dx, int dy);
bool tuftPixel(int dx, int dy);

}  // namespace shear_motion
