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
// ボタン・ブザーは Pico 右側ピン（H/I/J 列からアクセス可能）に変更
// ※ 左側 GPIO は Pico 本体が B-E 列を覆っているためジャンパー挿入不可
constexpr uint PIN_BTN_LEFT   = 21;  // 行 14 右側 (H14)
constexpr uint PIN_BTN_CENTER = 20;  // 行 15 右側 (H15)
constexpr uint PIN_BTN_RIGHT  = 19;  // 行 16 右側 (H16)
constexpr uint PIN_BUZZER     = 18;  // 行 17 右側 (H17)

constexpr uint32_t TICK_MS                = 50;            // ゲーム全体の tick 周期（通常時のループ周期でもある）
constexpr uint32_t MINI_LOOP_MS           = 5;             // ミニゲーム中のボタン読み取り・更新の周期
constexpr uint32_t MINI_DRAW_MS           = 33;            // ミニゲーム中の描画周期（約 30fps。I2C 転送に約 25ms かかる）
// 状態が変わった時の最短セーブ間隔（連打などで頻繁に書かないようにする rate limit）
constexpr uint32_t MIN_SAVE_INTERVAL_MS   = 30 * 1000;     // 30 秒
// 状態が変わってなくても age_ticks 進行を捕まえるための強制セーブ間隔。
// Pico フラッシュは ~100k 消去サイクル制限なので、30 分間隔なら 100k/(48/day) ≈ 5.7 年保つ。
constexpr uint32_t FORCE_SAVE_INTERVAL_MS = 30 * 60 * 1000; // 30 分
constexpr uint32_t LONG_PRESS_MS          = 500;           // 長押しと判定する閾値
constexpr uint32_t DEBOUNCE_MS            = 30;            // チャタリング除去の最小間隔

// ボタン状態（press 検出 + 長押し検出 + デバウンス）
struct ButtonState {
    bool     pressed         = false;
    uint32_t press_start_ms  = 0;
    uint32_t last_change_ms  = 0;
    bool     long_fired      = false;
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

        // チャタリング対策：状態変化は前回変化から DEBOUNCE_MS 以上経過したものだけ採用
        if (now_pressed != s.pressed && (now_ms - s.last_change_ms) < DEBOUNCE_MS) {
            return;
        }

        if (now_pressed && !s.pressed) {
            // 押下開始
            s.press_start_ms = now_ms;
            s.last_change_ms = now_ms;
            s.long_fired = false;
            s.pressed = true;
            game.onButton(btn);
        } else if (!now_pressed && s.pressed) {
            // 離した
            s.last_change_ms = now_ms;
            s.pressed = false;
        } else if (now_pressed && s.pressed && support_long && !s.long_fired) {
            if (now_ms - s.press_start_ms > LONG_PRESS_MS) {
                game.onButton(long_btn);
                s.long_fired = true;
            }
        }
    };

    uint32_t last_tick_ms = to_ms_since_boot(get_absolute_time());
    uint32_t last_draw_ms = 0;

    while (true) {
        const uint32_t now_ms = to_ms_since_boot(get_absolute_time());
        game.setNowMs(now_ms);
        sound.update(now_ms);        // blip の終了時刻を過ぎていたら止める

        // ボタン処理（active LOW、エッジ検出）。ミニゲーム中は約 5ms ごとに読む。
        poll_button(PIN_BTN_LEFT,   bL, Game::Button::LEFT,   true,  Game::Button::LEFT_LONG);
        poll_button(PIN_BTN_CENTER, bC, Game::Button::CENTER, false, Game::Button::CENTER);
        poll_button(PIN_BTN_RIGHT,  bR, Game::Button::RIGHT,  false, Game::Button::RIGHT);

        // ゲーム全体の tick（年齢・空腹など）は、ループの速さに関係なく経過時間で TICK_MS ごと。
        while (now_ms - last_tick_ms >= TICK_MS) {
            game.tick();
            last_tick_ms += TICK_MS;
        }
        game.updateMini();

        // 描画。通常は毎ループ、ミニゲーム中は約 33ms ごと。
        const bool mini = game.inMiniGame();
        // エンディング曲の再生中も、音符の切り替わりが遅れないよう、ミニゲームと同じ速さで回す
        // （通常の周期 + 描画の時間だと、音の始まりが 70ms ほどずれる）。保存は、ミニゲームのときだけ後回しにする。
        const bool fast = mini || sound.melodyPlaying();
        if (!fast || now_ms - last_draw_ms >= MINI_DRAW_MS) {
            disp.draw(game);
            oled.show();
            last_draw_ms = now_ms;
            // 待つ音（餌・撫でる・毛刈り・進化）は、画面を切り替えたあとに鳴らす。ボタン処理の中で鳴らすと、
            // 鳴り終わるまで描画が遅れて、メニューが表示されたまま音が聞こえてしまう。
            game.playPendingSfx();
        }

        // dirty なら最短間隔以上経過していれば保存、dirty でなくても force 間隔で保存。
        // フラッシュの書き込み中は数十 ms 入力が止まるので、ミニゲーム中は後回しにする。
        int64_t since_save_us = absolute_time_diff_us(last_save, get_absolute_time());
        bool min_elapsed   = since_save_us > int64_t(MIN_SAVE_INTERVAL_MS)   * 1000;
        bool force_elapsed = since_save_us > int64_t(FORCE_SAVE_INTERVAL_MS) * 1000;
        if (!mini && ((game.isDirty() && min_elapsed) || force_elapsed)) {
            save.write(game.saveData());
            game.clearDirty();
            last_save = get_absolute_time();
        }

        sleep_ms(fast ? MINI_LOOP_MS : TICK_MS);
    }
}
