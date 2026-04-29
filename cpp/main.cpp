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

constexpr uint32_t TICK_MS          = 50;     // 約 20 fps
constexpr uint32_t SAVE_INTERVAL_MS = 30000;  // 30 秒に 1 回フラッシュへ
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

    while (true) {
        game.tick();

        // ボタン処理（active LOW、簡易デバウンス）
        if (!gpio_get(PIN_BTN_LEFT))   { game.onButton(Game::Button::LEFT);   sleep_ms(200); }
        if (!gpio_get(PIN_BTN_CENTER)) { game.onButton(Game::Button::CENTER); sleep_ms(200); }
        if (!gpio_get(PIN_BTN_RIGHT))  { game.onButton(Game::Button::RIGHT);  sleep_ms(200); }

        disp.draw(game);
        oled.show();

        if (absolute_time_diff_us(last_save, get_absolute_time()) > int64_t(SAVE_INTERVAL_MS) * 1000) {
            GameSaveData sd = game.saveData();
            save.write(sd);
            last_save = get_absolute_time();
        }

        sleep_ms(TICK_MS);
    }
}
