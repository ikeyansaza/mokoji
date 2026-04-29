# ============================================================
# mokomoko - main.py
# エントリーポイント・ゲームループ
# ============================================================

from machine import Pin, I2C
import utime
from ssd1306 import SSD1306_I2C
from game import Game
from display import Display
from sound import Sound
from save import Save

# --- ハードウェア初期化 ---
i2c = I2C(0, scl=Pin(17), sda=Pin(16), freq=400000)
oled = SSD1306_I2C(128, 64, i2c)

BTN_LEFT   = Pin(10, Pin.IN, Pin.PULL_UP)
BTN_CENTER = Pin(11, Pin.IN, Pin.PULL_UP)
BTN_RIGHT  = Pin(12, Pin.IN, Pin.PULL_UP)

# --- モジュール初期化 ---
save    = Save()
sound   = Sound(pin=15)
disp    = Display(oled)
game    = Game(save.load())

# --- メインループ ---
TICK_MS = 50  # 約20fps

def handle_buttons():
    if not BTN_LEFT.value():
        game.on_button('left')
        utime.sleep_ms(200)
    if not BTN_CENTER.value():
        game.on_button('center')
        utime.sleep_ms(200)
    if not BTN_RIGHT.value():
        game.on_button('right')
        utime.sleep_ms(200)

last_save = utime.ticks_ms()
SAVE_INTERVAL = 30_000  # 30秒ごとに保存

while True:
    game.tick(TICK_MS)
    handle_buttons()
    disp.draw(game.state())
    oled.show()

    # 定期セーブ
    now = utime.ticks_ms()
    if utime.ticks_diff(now, last_save) > SAVE_INTERVAL:
        save.write(game.save_data())
        last_save = now

    utime.sleep_ms(TICK_MS)
