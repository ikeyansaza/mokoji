# メリノ（進化先から外した）— 没候補

モコ系の成体 MERINO（メリノ）の絵。進化のルールを、空腹・幸福の値と乱数で決める形に変えたとき、モコ系の進化先を
CORRIEDALE / LINCOLN の 2 つにして、MERINO を、一旦、進化先から外した。絵も `sprites.py`（ゲームのスプライト）から外して、
ここに残してある。

## 経緯

- 最初は、進化先から外すだけにして、絵と `Breed::MERINO` は残していた。
- そのあと、`oled_preview.html` の進化ツリー（game sprites）に、進化しない MERINO が並んでいると紛らわしいので、
  絵もゲームのスプライトから外して、没候補に移した。
- `Breed::MERINO`（セーブの値、名前の表示）は残してある。古いセーブに MERINO の羊がいても壊れず、絵は CORRIEDALE で代用する
  （`display.cpp`）。

## 候補の一覧

| ID | 特徴 | 状態 |
|---|---|---|
| MERINO | 通常版。顔がモコモコの毛で覆われ、目が見えなくなった成体（手描き） | 外した（進化先から外した） |
| MERINO_FLUFFY | 増毛版（通常版の左右の端の外側に 2px の毛） | 外した（進化先から外した） |

増毛の検討（F1〜F4、顔の覆い C1〜C3）は [../fluffy/](../fluffy/) に残っている。

## 復活させるとき

1. `candidates.txt` の `MERINO` を、`sprites.py` に `MERINO = ( ... )` として戻す。
2. `ADULT_MERINO_F` / `_L` / `_R` と、増毛版 `ADULT_MERINO_FLUFFY_F` / `_L` / `_R`（`_halo(MERINO)`）を作る。
3. `sprites_gen.py` の `SPRITE_NAMES` と `BREEDS` に戻し、`display.cpp` の MERINO の case を専用の絵に戻す。
4. `game.cpp` の `adultChoices`（モコ系）に MERINO を足す。
5. `python3 sprites_gen.py` で再生成して、テスト（`test_preview_data.py` の成体・増毛の数、`test_sprites.py` の `FLUFFY_BASE`）を戻す。

## 実際の見た目で確認する

`candidates.txt` の中身を、`oled_preview.html` の左の貼り付け欄に貼って parse する。
