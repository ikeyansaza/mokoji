#pragma once
#include "ssd1306.h"
#include "game.h"
#include "background.h"

class Display {
public:
    explicit Display(SSD1306* oled);
    void draw(const Game& g);

private:
    SSD1306* _oled;

    // 背景。毎フレーム関数を呼ぶ代わりに、起動時に列ごとのビットへ展開しておく。
    uint8_t  _grass_cols[background::W];      // bit b = y が GRASS_TOP + b の草
    uint32_t _sky_cols[2][background::W];     // bit y = 月・星（[0/1] は星のまたたきの 2 フレーム）
    uint32_t _futon_mask[background::W];      // bit b = y が FUTON_TOP + b の、布団が羊を覆う範囲
    uint32_t _futon_lit[background::W];       // 同じく、布団の縁と縫い目（点く）

    // 背景は、静止画のまま何時間も点灯し続けると OLED が焼き付くので、ゆっくり左右に動かして
    // 同じ場所を光らせ続けない（就寝画面のスプライトと同じ ~8 秒周期で ±2px）。
    static int backgroundDrift(const Game& g);
    void drawGrass(int dx);
    void drawFuton(int dx);
    void drawNightSky(int frame, int dx);
    void drawDaySky(const Game& g, int dx);

    void drawMain(const Game& g);
    void drawMenu(const Game& g);
    void drawSleep(const Game& g);
    void drawNameInput(const uint8_t* buffer);
    void drawMinigame(const Game& g);
    void drawProfile(const Game& g);
    void drawStatusBar(const Game& g, int x, int y, const char* label, int value);
    void drawGrave(const Game& g);
    void drawNaming(const Game& g);

    // 24x24 スプライト [24][3] のポインタを返す。
    const uint8_t (*selectSprite(const Game& g, Game::Face faceOverride))[3];
    void drawWool(int wool, int sx);
    void drawActionFx(const Game& g);
    void drawFaceFx(const Game& g, int sx, int sy);
};
