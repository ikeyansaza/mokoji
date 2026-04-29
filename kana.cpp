#include "kana.h"
#include <cstring>

namespace kana {

const Info TABLE[COUNT] = {
    /*  0 あ */ {"a"},   /*  1 い */ {"i"},   /*  2 う */ {"u"},
    /*  3 え */ {"e"},   /*  4 お */ {"o"},
    /*  5 か */ {"ka"},  /*  6 き */ {"ki"},  /*  7 く */ {"ku"},
    /*  8 け */ {"ke"},  /*  9 こ */ {"ko"},
    /* 10 さ */ {"sa"},  /* 11 し */ {"shi"}, /* 12 す */ {"su"},
    /* 13 せ */ {"se"},  /* 14 そ */ {"so"},
    /* 15 た */ {"ta"},  /* 16 ち */ {"chi"}, /* 17 つ */ {"tsu"},
    /* 18 て */ {"te"},  /* 19 と */ {"to"},
    /* 20 な */ {"na"},  /* 21 に */ {"ni"},  /* 22 ぬ */ {"nu"},
    /* 23 ね */ {"ne"},  /* 24 の */ {"no"},
    /* 25 は */ {"ha"},  /* 26 ひ */ {"hi"},  /* 27 ふ */ {"fu"},
    /* 28 へ */ {"he"},  /* 29 ほ */ {"ho"},
    /* 30 ま */ {"ma"},  /* 31 み */ {"mi"},  /* 32 む */ {"mu"},
    /* 33 め */ {"me"},  /* 34 も */ {"mo"},
    /* 35 や */ {"ya"},  /* 36 ゆ */ {"yu"},  /* 37 よ */ {"yo"},
    /* 38 ら */ {"ra"},  /* 39 り */ {"ri"},  /* 40 る */ {"ru"},
    /* 41 れ */ {"re"},  /* 42 ろ */ {"ro"},
    /* 43 わ */ {"wa"},  /* 44 を */ {"wo"},  /* 45 ん */ {"n"},
    /* 46 が */ {"ga"},  /* 47 ぎ */ {"gi"},  /* 48 ぐ */ {"gu"},
    /* 49 げ */ {"ge"},  /* 50 ご */ {"go"},
    /* 51 ざ */ {"za"},  /* 52 じ */ {"ji"},  /* 53 ず */ {"zu"},
    /* 54 ぜ */ {"ze"},  /* 55 ぞ */ {"zo"},
    /* 56 だ */ {"da"},  /* 57 ぢ */ {"di"},  /* 58 づ */ {"du"},
    /* 59 で */ {"de"},  /* 60 ど */ {"do"},
    /* 61 ば */ {"ba"},  /* 62 び */ {"bi"},  /* 63 ぶ */ {"bu"},
    /* 64 べ */ {"be"},  /* 65 ぼ */ {"bo"},
    /* 66 ぱ */ {"pa"},  /* 67 ぴ */ {"pi"},  /* 68 ぷ */ {"pu"},
    /* 69 ぺ */ {"pe"},  /* 70 ぽ */ {"po"},
    /* 71 っ */ {"_t"},  /* 72 ょ */ {"_yo"}, /* 73 ゅ */ {"_yu"},
    /* 74 ぁ */ {"_a"},
};

const Row ROWS[ROW_COUNT] = {
    {"a-",   0, 5},
    {"ka-",  5, 5},
    {"sa-", 10, 5},
    {"ta-", 15, 5},
    {"na-", 20, 5},
    {"ha-", 25, 5},
    {"ma-", 30, 5},
    {"ya-", 35, 3},
    {"ra-", 38, 5},
    {"wa-", 43, 3},
    {"ga-", 46, 5},
    {"za-", 51, 5},
    {"da-", 56, 5},
    {"ba-", 61, 5},
    {"pa-", 66, 5},
    {"x-",  71, 4},   // 小文字
};

// プリセット名（20 個）— 仕様の hiragana を index に変換したもの。
const uint8_t PRESETS[PRESET_COUNT][NAME_BUF_LEN] = {
    {34,  9, END, END, END},  //  0 もこ
    {27, 43, END, END, END},  //  1 ふわ
    { 7, 34, END, END, END},  //  2 くも
    {36,  6, END, END, END},  //  3 ゆき
    {14, 38, END, END, END},  //  4 そら
    {43, 15, END, END, END},  //  5 わた
    { 6, 39, END, END, END},  //  6 きり
    {17, 36, END, END, END},  //  7 つゆ
    {25, 20, END, END, END},  //  8 はな
    {32, 47, END, END, END},  //  9 むぎ
    {70, 70, END, END, END},  // 10 ぽぽ
    {30, 40, END, END, END},  // 11 まる
    {11, 42, END, END, END},  // 12 しろ
    {27,  7, END, END, END},  // 13 ふく
    {24, 24, END, END, END},  // 14 のの
    {33, 33, END, END, END},  // 15 めめ
    {34, 34, END, END, END},  // 16 もも
    { 9, 32, END, END, END},  // 17 こむ
    {26, 17, END, END, END},  // 18 ひつ
    {43,  5, END, END, END},  // 19 わか
};

int toRomaji(const uint8_t* name_kana, char* out, int out_size) {
    if (out_size <= 0) return 0;
    int p = 0;
    for (int i = 0; i < MAX_NAME; ++i) {
        uint8_t k = name_kana[i];
        if (k == END || k >= COUNT) break;
        const char* r = TABLE[k].romaji;
        while (*r && p < out_size - 1) {
            out[p++] = *r++;
        }
        if (p >= out_size - 1) break;
    }
    out[p] = '\0';
    return p;
}

}  // namespace kana
