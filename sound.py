# ============================================================
# mokomoko - sound.py
# パッシブブザー効果音（PWM制御）
# ============================================================

from machine import Pin, PWM
import utime


class Sound:
    def __init__(self, pin=15):
        self.pwm = PWM(Pin(pin))
        self.volume = 512  # 0-65535（duty）

    def _beep(self, freq, duration_ms):
        self.pwm.freq(freq)
        self.pwm.duty_u16(self.volume)
        utime.sleep_ms(duration_ms)
        self.pwm.duty_u16(0)

    def mee(self):
        """メェ〜（なでる）"""
        self._beep(440, 80)
        utime.sleep_ms(20)
        self._beep(494, 120)
        utime.sleep_ms(20)
        self._beep(440, 200)

    def mog(self):
        """もぐもぐ（餌やり）"""
        for _ in range(3):
            self._beep(300, 50)
            utime.sleep_ms(30)

    def joki(self):
        """ジョキジョキ（毛刈り）"""
        for f in [600, 500, 600, 500]:
            self._beep(f, 60)
            utime.sleep_ms(20)

    def happy(self):
        """喜び（イベント）"""
        for f in [523, 659, 784]:
            self._beep(f, 100)
            utime.sleep_ms(30)

    def set_volume(self, vol):
        """0〜10で音量設定"""
        self.volume = int(vol * 6553)
