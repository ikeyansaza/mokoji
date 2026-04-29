# MOKOJI 🐑

> モコモコな羊の略。めん羊育成デバイス。

Raspberry Pi Pico + OLED で動く、たまごっち型の羊育成ゲーム。
お世話の仕方によって毎回違う品種の羊に育つ。

C++ (Pico SDK) で実装。

---

## コンセプト

- 「同じ育て方でも毎回違う子になる」体験を重視
- 動物はコントロール可能な奇跡の存在、というテーマ
- 将来的には動物図鑑・環世界エクスプローラーと連携する三層構造の一部

---

## ハードウェア

| パーツ              | ピン                              | 備考                       |
|---------------------|-----------------------------------|----------------------------|
| Raspberry Pi Pico   | —                                 | RP2040                     |
| OLED 0.96" 128×64   | SDA=GP16 / SCL=GP17 / 0x3C        | SSD1306, I2C 400kHz        |
| ボタン左            | GP10                              | 内部プルアップ・active LOW |
| ボタン中央          | GP11                              | 同上                       |
| ボタン右            | GP12                              | 同上                       |
| パッシブブザー      | GP15                              | PWM 制御                   |

外装は白いフェルトで手作り予定。モコモコ。

---

## 進化システム

```
幼年（共通の子羊）
    ↓ お世話の傾向 × ランダム
若羊期（見た目で2パターンに分岐）
  ├── もこもこ型 → コリデール / メリノ
  ├── すらっと型 → サフォーク / サウスダウン
  └── レア型    → イーストフリーシアン
```

- 進化先はプレイヤーに見せない
- 「なんとなくもこもこ系かな…？」と感じながら育てる

---

## ビルド

### 必要なもの

- [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk)（`PICO_SDK_PATH` 環境変数で指定）
- ARM GNU Toolchain（`brew install --cask gcc-arm-embedded` 推奨）
- CMake 3.13 以上
- picotool（`brew install picotool`）

### コマンド

```bash
cmake -B build -DPICO_SDK_PATH=$HOME/pico-sdk
cmake --build build -j
```

成果物は `build/mokoji.uf2`。Pico を BOOTSEL 押しながら接続して D&D で書き込み、または：

```bash
picotool load -x build/mokoji.uf2
```

### 進化検証用の高速モード

ゲーム内 1 時間を 3 秒に短縮するビルドオプション：

```bash
cmake -B build_fast -DPICO_SDK_PATH=$HOME/pico-sdk -DDEBUG_FAST=ON
cmake --build build_fast -j
```

これで lamb→young 進化が約 3.6 分、young→adult が約 8.4 分で確認できる。

---

## ホスト側ユニットテスト

`game.cpp` のロジック（進化・ステータス減衰・save/load 等）は
ハード非依存なので Mac で直接検証できる。

```bash
cd test
make test
```

8 ケース、毎回 1 秒未満で完走する。

---

## ファイル構成

```
mokoji/
├── CMakeLists.txt           # Pico SDK ビルド設定
├── pico_sdk_import.cmake    # SDK 導入用ボイラープレート
├── main.cpp                 # エントリ・ハードウェア初期化・ゲームループ
├── game.{h,cpp}             # ゲームロジック・進化・睡眠・墓
├── display.{h,cpp}          # OLED 描画
├── ssd1306.{h,cpp}          # SSD1306 OLED ドライバ最小実装
├── font.{h,cpp}             # 5×7 ASCII フォント
├── sound.{h,cpp}            # PWM 効果音
├── save.{h,cpp}             # フラッシュ末尾セクタへバイナリ保存
├── sprites.h                # 27 枚のスプライト（auto-generated）
└── test/                    # ホスト側ユニットテスト
    ├── Makefile
    ├── test_game.cpp
    └── stubs/               # Pico SDK 非依存にする stub 群
```

---

## ロードマップ

| フェーズ | 内容                                                  |
|----------|-------------------------------------------------------|
| v0.1     | ハードウェア組み立て・環境構築                         |
| v0.2     | 起動画面・ドット絵表示・ボタン確認                     |
| v0.3     | ゲームロジック（空腹・幸福・餌・睡眠）                 |
| v0.4     | 効果音・ミニゲーム・進化・お墓                         |
| v1.0     | フェルト外装完成                                       |
| v2.0     | 電池対応・品種拡張・Pico W への移行検討               |
| v3.0     | 動物図鑑・環世界エクスプローラー連携                   |

---

## License

Private / All rights reserved.
