# ============================================================
# mokomoko - save.py
# フラッシュメモリ読み書き（ujson）
# ============================================================

import ujson

SAVE_PATH = '/mokoji_save.json'


class Save:
    def load(self):
        try:
            with open(SAVE_PATH, 'r') as f:
                return ujson.loads(f.read())
        except:
            return None

    def write(self, data):
        try:
            with open(SAVE_PATH, 'w') as f:
                f.write(ujson.dumps(data))
        except Exception as e:
            print('Save error:', e)
