#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "ssd1306.h"
#include "game.h"
#include "display.h"
#include "sound.h"
#include "save.h"

namespace {
constexpr uint PIN_SDA        = 16;
constexpr uint PIN_SCL        = 17;
constexpr uint PIN_BTN_LEFT   = 10;
constexpr uint PIN_BTN_CENTER = 11;
constexpr uint PIN_BTN_RIGHT  = 12;
constexpr uint PIN_BUZZER     = 15;

constexpr uint32_t TICK_MS                = 50;            // 約 20 fps
// 状態が変わった時の最短セーブ間隔（連打などで頻繁に書かないようにする rate limit）
constexpr uint32_t MIN_SAVE_INTERVAL_MS   = 30 * 1000;     // 30 秒
// 状態が変わってなくても age_ticks 進行を捕まえるための強制セーブ間隔。
// Pico フラッシュは ~100k 消去サイクル制限なので、30 分間隔なら 100k/(48/day) ≈ 5.7 年保つ。
constexpr uint32_t FORCE_SAVE_INTERVAL_MS = 30 * 60 * 1000; // 30 分
constexpr uint32_t LONG_PRESS_MS          = 500;           // 長押しと判定する閾値

// ボタン状態（press 検出 + 長押し検出）
struct ButtonState {
    bool     pressed       = false;
    uint32_t press_start_ms = 0;
    bool     long_fired    = false;
};
}  // namespace

static void setupButton(uint pin) {
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_pull_up(pin);
}

int main() {
    stdio_init_all();

    // I2C0: SDA=GP16, SCL=GP17, 400kHz
    i2c_init(i2c0, 400 * 1000);
    gpio_set_function(PIN_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(PIN_SDA);
    gpio_pull_up(PIN_SCL);

    setupButton(PIN_BTN_LEFT);
    setupButton(PIN_BTN_CENTER);
    setupButton(PIN_BTN_RIGHT);

    SSD1306 oled(i2c0);
    oled.begin();

    Save save;
    Sound sound(PIN_BUZZER);
    GameSaveData saved;
    Game game(&sound, save.load(&saved) ? &saved : nullptr);
    Display disp(&oled);

    absolute_time_t last_save = get_absolute_time();

    ButtonState bL, bC, bR;
    auto poll_button = [&](uint pin, ButtonState& s, Game::Button btn,
                           bool support_long, Game::Button long_btn) {
        bool now_pressed = !gpio_get(pin);
        uint32_t now_ms  = to_ms_since_boot(get_absolute_time());
        if (now_pressed && !s.pressed) {
            // 押下開始
            s.press_start_ms = now_ms;
            s.long_fired = false;
            game.onButton(btn);
        } else if (now_pressed && s.pressed && support_long && !s.long_fired) {
            if (now_ms - s.press_start_ms > LONG_PRESS_MS) {
                game.onButton(long_btn);
                s.long_fired = true;
            }
        }
        s.pressed = now_pressed;
    };

    while (true) {
        game.tick();

        // ボタン処理（active LOW、エッジ検出。50ms tick で簡易デバウンス）
        poll_button(PIN_BTN_LEFT,   bL, Game::Button::LEFT,   true,  Game::Button::LEFT_LONG);
        poll_button(PIN_BTN_CENTER, bC, Game::Button::CENTER, false, Game::Button::CENTER);
        poll_button(PIN_BTN_RIGHT,  bR, Game::Button::RIGHT,  false, Game::Button::RIGHT);

        disp.draw(game);
        oled.show();

        // dirty なら最短間隔以上経過していれば保存、dirty でなくても force 間隔で保存
        int64_t since_save_us = absolute_time_diff_us(last_save, get_absolute_time());
        bool min_elapsed   = since_save_us > int64_t(MIN_SAVE_INTERVAL_MS)   * 1000;
        bool force_elapsed = since_save_us > int64_t(FORCE_SAVE_INTERVAL_MS) * 1000;
        if ((game.isDirty() && min_elapsed) || force_elapsed) {
            save.write(game.saveData());
            game.clearDirty();
            last_save = get_absolute_time();
        }

        sleep_ms(TICK_MS);
    }
}
