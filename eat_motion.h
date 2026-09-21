#pragma once

// ご飯（草を食べるモーション）。Pico SDK 非依存の純粋な関数で、Display が見て描く。
//
// 羊のスプライトは正面向きの絵（左右向きの絵も左右対称）しかなく、頭を下げる絵がない。そこで、
// 「草のほうへ体を傾けてうなずく」ことで食べているように見せる。草の房は、食べるたびに短くなり、
// 3 回のぱくっでなくなる。動きは、ご飯の動きの経過フレーム t（Game::walkTick、0〜40）だけで決まる。
namespace eat_motion {

constexpr int BITE_COUNT = 3;
constexpr int BITE_START[BITE_COUNT] = { 4, 16, 28 };   // ぱくっの開始フレーム
constexpr int BITE_LEN   = 5;                            // ぱくっの間、うなずく
constexpr int LEAN_PX    = 3;                            // 草のほうへ傾く量（右または左へ・下へ）

constexpr int TUFT_W   = 5;    // 草の房の幅（葉は 3 本、dx = 0, 2, 4）
constexpr int TUFT_H   = 6;    // 一番高い葉の高さ
constexpr int GROUND_Y = 55;   // 草の根元の y（草の地面の帯 y=56〜 の真上）

// 草を置く向き。羊が右半分（walk_x > 56）にいるときは左に置く（画面から、はみ出さないため）。
int  direction(int walk_x);              // +1：右に草  /  -1：左に草

bool isBiting(int t);                    // ぱくっの最中か
int  bitesDone(int t);                   // 食べ終えたぱくっの回数（0〜BITE_COUNT）

// ぱくっの最中は、草の向き dir へ傾く（右下 / 左下）。それ以外は 0。
void lean(int t, int dir, int* dx, int* dy);

// 草の房の左端の x。羊の 2 倍スプライト（48px 幅）の外に置く。
int  tuftLeft(int walk_x);

// 草の葉。dx は房の左端から（0〜TUFT_W-1）、k は根元からの高さ（1 が根元の行 = GROUND_Y、上へ増える）。
// k = 0 には葉がない。
// bites は食べ終えた回数：短くなり、BITE_COUNT 回でなくなる。範囲外の回数は、負なら 0、超えたらなし。
bool tuftPixel(int dx, int k, int bites);

}  // namespace eat_motion
