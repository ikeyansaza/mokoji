#pragma once

// ご飯（干し草ロールを食べるモーション）。Pico SDK 非依存の純粋な関数で、Display が見て描く。
//
// 羊のスプライトは正面向きの絵（左右向きの絵も左右対称）しかなく、頭を下げる絵がない。そこで、
// 「ロールのほうへ体を傾けてうなずく」ことで食べているように見せる。ロールは、ぱくっのたびに
// 羊のいる側から丸くかじり取られ、3 回でなくなる。動きは、ご飯の動きの経過フレーム t
// （Game::walkTick、0〜Game::FEED_ACTION_TICKS）だけで決まる。
namespace eat_motion {

constexpr int BITE_COUNT = 3;
constexpr int BITE_START[BITE_COUNT] = { 12, 32, 52 };  // ぱくっの開始フレーム（1 秒 = 20 フレームおき）
constexpr int BITE_LEN   = 5;                            // ぱくっの間、うなずく

// ご飯の音（Sound::mog）は、鳴り終わるまで待つ作りで、約 0.3 秒（6 フレーム）のあいだ画面が止まる。
// 止まっている間に時間が過ぎるので、1 回目のぱくっは、音が終わってから、ロールを見つける間
// （LOOK_TICKS）をおいて始める。
constexpr int SOUND_BLOCK_TICKS = 6;
constexpr int LOOK_TICKS        = 4;

constexpr int LEAN_PX    = 3;                            // ロールのほうへ傾く量（右または左へ・下へ）

constexpr int BALE_W   = 12;   // 干し草ロールの大きさ
constexpr int BALE_H   = 12;
constexpr int GROUND_Y = 55;   // ロールの根元（最下行）の y。草の地面の帯（y=56〜）の真上

// ロールを置く向き。羊が右半分（walk_x > 56）にいるときは左に置く（画面から、はみ出さないため）。
int  direction(int walk_x);              // +1：右にロール  /  -1：左にロール

bool isBiting(int t);                    // ぱくっの最中か
int  bitesTaken(int t);                  // かじった回数（ぱくっが始まった数、0〜BITE_COUNT）

// ぱくっの最中は、ロールの向き dir へ傾く（右下 / 左下）。それ以外は 0。
void lean(int t, int dir, int* dx, int* dy);

// ロールの左端の x。羊の 2 倍スプライト（48px 幅）の外に置く。
int  baleLeft(int walk_x);

// 干し草ロール（同心円の輪と渦巻き）。dx, dy はロールの左上から（0〜BALE_W-1, 0〜BALE_H-1）。
// bites はかじった回数：羊のいる側（dir が +1 なら左側、-1 なら右側）から丸くかじり取られ、
// BITE_COUNT 回でなくなる。範囲外の回数は、負なら 0、超えたらなし。
bool balePixel(int dx, int dy, int bites, int dir);

}  // namespace eat_motion
