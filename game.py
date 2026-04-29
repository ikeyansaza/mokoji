# ============================================================
# mokomoko - game.py
# ゲームロジック（ステータス・進化・睡眠・うろうろ）
# ============================================================

import urandom
import utime

# 進化テーブル（傾向スコアに基づく確率マップ）
# 傾向: feed=餌やり, pet=なでる, shear=毛刈り
EVOLUTION = {
    # (stage, type) -> [(breed, weight), ...]
    ('lamb', None): [
        ('moko', 50),   # もこもこ型
        ('sura', 50),   # すらっと型
        ('rare', 5),    # レア（別途低確率で上書き）
    ],
    ('young_moko', 'moko'): [
        ('corriedale', 60),
        ('merino', 40),
    ],
    ('young_sura', 'sura'): [
        ('suffolk', 50),
        ('southdown', 50),
    ],
    ('young_rare', 'rare'): [
        ('eastfriesian', 100),
    ],
}

BREED_TENDENCY = {
    'corriedale': 'feed',
    'merino':     'pet',
    'suffolk':    'balance',
    'southdown':  'shear',
    'eastfriesian': 'rare',
}

RARE_PROB = 5  # レア出現確率 (%)

class Game:
    def __init__(self, data=None):
        if data:
            self.name       = data.get('name', 'もこ')
            self.breed      = data.get('breed', None)
            self.stage      = data.get('stage', 'lamb')
            self.sheep_type = data.get('sheep_type', None)
            self.hunger     = data.get('hunger', 100)
            self.happy      = data.get('happy', 100)
            self.sleepy     = data.get('sleepy', 0)
            self.wool       = data.get('wool', 0)
            self.age_ticks  = data.get('age_ticks', 0)
            self.graves     = data.get('graves', [])
            self.tendency   = data.get('tendency', {'feed':0,'pet':0,'shear':0})
            self.sleeping   = data.get('sleeping', False)
        else:
            self._new_game()

        # うろうろ用
        self._walk_x    = 52
        self._walk_dir  = 1   # 1=右, -1=左
        self._walk_tick = 0
        self._face      = 'right'  # 向き
        self._action    = None     # なでる・餌やりのリアクション

        # メニュー
        self._menu_items  = ['feed', 'pet', 'shear', 'mini']
        self._menu_cursor = 0
        self._screen      = 'main'  # main / menu / minigame / grave

    def _new_game(self):
        self.name       = 'もこ'
        self.breed      = None
        self.stage      = 'lamb'
        self.sheep_type = None
        self.hunger     = 100
        self.happy      = 100
        self.sleepy     = 0
        self.wool       = 0
        self.age_ticks  = 0
        self.graves     = []
        self.tendency   = {'feed': 0, 'pet': 0, 'shear': 0}
        self.sleeping   = False

    # --------------------------------------------------------
    # メインティック（50ms毎に呼ばれる）
    # --------------------------------------------------------
    TICKS_PER_SEC  = 20
    TICKS_PER_MIN  = 20 * 60
    TICKS_PER_HOUR = 20 * 60 * 60
    TICKS_PER_DAY  = 20 * 60 * 60 * 24

    def tick(self, ms):
        self.age_ticks += 1

        # 睡眠中はステータス変化を抑制
        rate = 0.2 if self.sleeping else 1.0

        # ステータス時間経過（1時間に約1ポイント減少）
        if self.age_ticks % self.TICKS_PER_HOUR == 0:
            self.hunger  = max(0, self.hunger  - int(1 * rate))
            self.happy   = max(0, self.happy   - int(1 * rate))
            self.wool    = min(100, self.wool  + 2)

        # 眠気の更新
        hour = (self.age_ticks // self.TICKS_PER_HOUR) % 24
        if 22 <= hour or hour < 6:
            self.sleepy = min(100, self.sleepy + 1)
        else:
            if self.sleeping:
                self.sleepy = max(0, self.sleepy - 2)

        # 睡眠遷移
        if not self.sleeping and self.sleepy >= 80:
            self.sleeping = True
        if self.sleeping and self.sleepy <= 10:
            self.sleeping = False

        # 死亡チェック
        if self.hunger <= 0 or self.happy <= 0:
            self._die()
            return

        # 進化チェック（3日目・7日目）
        age_days = self.age_ticks // self.TICKS_PER_DAY
        if self.stage == 'lamb' and age_days >= 3:
            self._evolve_to_young()
        elif self.stage in ('young_moko', 'young_sura', 'young_rare') and age_days >= 7:
            self._evolve_to_adult()

        # うろうろ更新
        if not self.sleeping:
            self._update_walk()

    # --------------------------------------------------------
    # うろうろ
    # --------------------------------------------------------
    def _update_walk(self):
        self._walk_tick += 1
        if self._action:
            # リアクション中は正面
            self._face = 'front'
            if self._walk_tick > 40:
                self._action = None
        else:
            if self._walk_tick % 3 == 0:
                self._walk_x += self._walk_dir
                self._face = 'right' if self._walk_dir > 0 else 'left'
                # 壁で折り返し
                if self._walk_x > 90:
                    self._walk_dir = -1
                elif self._walk_x < 10:
                    self._walk_dir = 1

    # --------------------------------------------------------
    # ボタン処理
    # --------------------------------------------------------
    def on_button(self, btn):
        if self.sleeping:
            return  # 睡眠中は操作不可

        if self._screen == 'main':
            if btn == 'center':
                self._screen = 'menu'
        elif self._screen == 'menu':
            if btn == 'left':
                self._menu_cursor = (self._menu_cursor - 1) % len(self._menu_items)
            elif btn == 'right':
                self._menu_cursor = (self._menu_cursor + 1) % len(self._menu_items)
            elif btn == 'center':
                self._do_action(self._menu_items[self._menu_cursor])
                self._screen = 'main'

    def _do_action(self, action):
        if action == 'feed':
            self.hunger = min(100, self.hunger + 30)
            self.tendency['feed'] += 1
            self._action = 'feed'
        elif action == 'pet':
            self.happy = min(100, self.happy + 20)
            self.tendency['pet'] += 1
            self._action = 'pet'
        elif action == 'shear':
            if self.wool > 10:
                self.wool = 0
                self.happy = min(100, self.happy + 10)
                self.tendency['shear'] += 1
                self._action = 'shear'
        elif action == 'mini':
            self._screen = 'minigame'
        self._walk_tick = 0

    # --------------------------------------------------------
    # 進化
    # --------------------------------------------------------
    def _evolve_to_young(self):
        # レアチェック
        if urandom.getrandbits(7) < RARE_PROB:
            self.stage = 'young_rare'
            self.sheep_type = 'rare'
            return

        # 傾向スコアで重み調整
        feed  = self.tendency['feed']
        pet   = self.tendency['pet']
        shear = self.tendency['shear']
        total = feed + pet + shear + 1

        moko_w = 50 + int((feed + pet) / total * 30)
        sura_w = 50 + int(shear / total * 30)

        roll = urandom.getrandbits(7) % (moko_w + sura_w)
        if roll < moko_w:
            self.stage = 'young_moko'
            self.sheep_type = 'moko'
        else:
            self.stage = 'young_sura'
            self.sheep_type = 'sura'

    def _evolve_to_adult(self):
        t = self.sheep_type
        feed  = self.tendency['feed']
        pet   = self.tendency['pet']
        shear = self.tendency['shear']
        total = feed + pet + shear + 1

        if t == 'moko':
            cor_w = 50 + int(feed / total * 50)
            mer_w = 50 + int(pet  / total * 50)
            roll = urandom.getrandbits(8) % (cor_w + mer_w)
            self.breed = 'corriedale' if roll < cor_w else 'merino'
        elif t == 'sura':
            suf_w = 50 + int((feed + pet) / total * 30)
            sou_w = 50 + int(shear / total * 50)
            roll = urandom.getrandbits(8) % (suf_w + sou_w)
            self.breed = 'suffolk' if roll < suf_w else 'southdown'
        else:
            self.breed = 'eastfriesian'

        self.stage = 'adult'

    # --------------------------------------------------------
    # 死亡・お墓
    # --------------------------------------------------------
    def _die(self):
        age_days = self.age_ticks // self.TICKS_PER_DAY
        self.graves.append({
            'name':  self.name,
            'breed': self.breed or self.stage,
            'age':   age_days,
        })
        self._new_game()
        self._screen = 'grave'

    # --------------------------------------------------------
    # 状態取得（display.py用）
    # --------------------------------------------------------
    def state(self):
        return {
            'screen':    self._screen,
            'name':      self.name,
            'stage':     self.stage,
            'breed':     self.breed,
            'sheep_type': self.sheep_type,
            'hunger':    self.hunger,
            'happy':     self.happy,
            'wool':      self.wool,
            'sleeping':  self.sleeping,
            'face':      self._face,
            'walk_x':    self._walk_x,
            'action':    self._action,
            'menu_cursor': self._menu_cursor,
            'menu_items':  self._menu_items,
            'graves':    self.graves,
        }

    def save_data(self):
        return {
            'name':      self.name,
            'breed':     self.breed,
            'stage':     self.stage,
            'sheep_type': self.sheep_type,
            'hunger':    self.hunger,
            'happy':     self.happy,
            'sleepy':    self.sleepy,
            'wool':      self.wool,
            'age_ticks': self.age_ticks,
            'graves':    self.graves,
            'tendency':  self.tendency,
            'sleeping':  self.sleeping,
        }
