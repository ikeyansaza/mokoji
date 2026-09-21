#pragma once
#include "ssd1306.h"
#include "game.h"

class Display {
public:
    explicit Display(SSD1306* oled);
    void draw(const Game& g);

private:
    SSD1306* _oled;

    void drawMain(const Game& g);
    void drawMenu(const Game& g);
    void drawSleep(const Game& g);
    void drawNameInput(const uint8_t* buffer);
    void drawMinigame(const Game& g);
    void drawStatusBar(const Game& g, int x, int y, const char* label, int value);
    void drawGrave(const Game& g);
    void drawNaming(const Game& g);

    // 24x24 スプライト [24][3] のポインタを返す。
    const uint8_t (*selectSprite(const Game& g, Game::Face faceOverride))[3];
    void drawWool(int wool, int sx);
    void drawActionFx(const Game& g);
    void drawFaceFx(const Game& g, int sx, int sy);
};
