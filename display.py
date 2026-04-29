# ============================================================
# mokomoko - display.py
# OLED描画ユーティリティ
# ============================================================

import sprites

MENU_LABELS = {
    'feed':  'えさ',
    'pet':   'なでる',
    'shear': '毛刈り',
    'mini':  'ゲーム',
}

class Display:
    def __init__(self, oled):
        self.oled = oled

    def draw(self, state):
        self.oled.fill(0)
        screen = state['screen']

        if state['sleeping']:
            self._draw_sleep(state)
        elif screen == 'main':
            self._draw_main(state)
        elif screen == 'menu':
            self._draw_menu(state)
        elif screen == 'grave':
            self._draw_grave(state)

    # --------------------------------------------------------
    # メイン画面
    # --------------------------------------------------------
    def _draw_main(self, state):
        # ステータスバー（上段）
        hunger_bars = state['hunger'] // 20  # 0-5
        happy_bars  = state['happy']  // 20
        self.oled.text('H:' + '|' * hunger_bars + '.' * (5 - hunger_bars), 0, 0)
        self.oled.text(state['name'], 100, 0)

        # 羊スプライト
        sprite = self._get_sprite(state)
        self._draw_sprite(sprite, state['walk_x'], 20)

        # 毛モコモコオーバーレイ
        if state['wool'] > 30:
            self._draw_wool(state['wool'], state['walk_x'])

        # 下段メニューヒント
        self.oled.text('< menu >', 40, 56)

    # --------------------------------------------------------
    # メニュー画面
    # --------------------------------------------------------
    def _draw_menu(self, state):
        items  = state['menu_items']
        cursor = state['menu_cursor']
        for i, item in enumerate(items):
            label = MENU_LABELS.get(item, item)
            x = i * 32
            if i == cursor:
                self.oled.fill_rect(x, 52, 30, 12, 1)
                self.oled.text(label[:3], x + 1, 54, 0)
            else:
                self.oled.text(label[:3], x + 1, 54, 1)

        # 羊は中央固定・正面
        sprite = self._get_sprite({**state, 'face': 'front'})
        self._draw_sprite(sprite, 52, 20)

    # --------------------------------------------------------
    # 睡眠画面
    # --------------------------------------------------------
    def _draw_sleep(self, state):
        # 画面を暗く（格子状に半分消す）
        for y in range(0, 64, 2):
            for x in range(0, 128, 2):
                self.oled.pixel(x, y, 0)
        sprite = sprites.LAMB_F  # とりあえず子羊正面で代用
        self._draw_sprite(sprite, 52, 20)
        self.oled.text('zzz...', 56, 48)

    # --------------------------------------------------------
    # お墓画面
    # --------------------------------------------------------
    def _draw_grave(self, state):
        self.oled.text('  + Memories +', 0, 0)
        graves = state['graves'][-3:]  # 最新3件
        for i, g in enumerate(graves):
            line = '{} {} {}d'.format(g['name'], g.get('breed','?')[:4], g['age'])
            self.oled.text(line, 0, 16 + i * 12)

    # --------------------------------------------------------
    # スプライト選択
    # --------------------------------------------------------
    def _get_sprite(self, state):
        stage = state['stage']
        breed = state['breed']
        st    = state.get('sheep_type')
        face  = state.get('face', 'front')

        suffix = {'left': '_L', 'right': '_R', 'front': '_F'}.get(face, '_F')

        if stage == 'lamb':
            base = 'LAMB'
        elif stage == 'young_moko':
            base = 'YOUNG_MOKO'
        elif stage == 'young_sura':
            base = 'YOUNG_SURA'
        elif stage == 'young_rare':
            base = 'YOUNG_RARE'
        elif stage == 'adult':
            mapping = {
                'corriedale': 'ADULT_COR',
                'merino':     'ADULT_MER',
                'suffolk':    'ADULT_SUF',
                'southdown':  'ADULT_SOU',
                'eastfriesian': 'ADULT_EAST',
            }
            base = mapping.get(breed, 'LAMB')
        else:
            base = 'LAMB'

        attr = base + suffix
        return getattr(sprites, attr, sprites.LAMB_F)

    # --------------------------------------------------------
    # スプライト描画
    # --------------------------------------------------------
    def _draw_sprite(self, sprite, x, y):
        oled = self.oled
        for row in range(24):
            b = sprite[row]  # [byte0, byte1, byte2]
            for col in range(24):
                bi = col // 8
                bit = 7 - (col % 8)
                if (b[bi] >> bit) & 1:
                    oled.pixel(x + col, y + row, 1)

    # --------------------------------------------------------
    # 毛モコモコオーバーレイ
    # --------------------------------------------------------
    def _draw_wool(self, wool, sx):
        # woolの値に応じて羊の周りにドットを散らす
        density = wool // 10  # 0-10
        import urandom
        for _ in range(density * 8):
            dx = urandom.getrandbits(5) - 12
            dy = urandom.getrandbits(5) - 12
            px = sx + 12 + dx
            py = 32 + dy
            if 0 <= px < 128 and 0 <= py < 64:
                self.oled.pixel(px, py, 1)
