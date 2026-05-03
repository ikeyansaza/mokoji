#pragma once
#include <cstdint>

constexpr int MAX_GRAVES        = 50;     // FIFO で古い墓から削除
constexpr int SAVE_NAME_KANA_LEN = 5;     // ひらがな最大 4 字 + END 終端

// 死因（お墓メッセージの分岐用）
namespace DeathCauseValue {
    constexpr uint8_t STATUS = 0;   // 餓死 / 不幸死
    constexpr uint8_t AGE    = 1;   // 天寿
}

struct GraveRecord {
    char     name[8];                          // 名前の romaji 表示（display 互換）
    uint8_t  name_kana[SAVE_NAME_KANA_LEN];    // 名前のひらがな index 列（kana::END 終端）
    char     breed[16];
    uint8_t  age_days;                         // 寿命 max 15 なので uint8_t で十分
    uint8_t  death_cause;                      // DeathCauseValue::STATUS or AGE
};

// フラッシュに書き込むセーブデータ。サイズが変わったら MAGIC を変えて互換性を切る。
// 末尾の crc は load 時に検証されるので、部分書き込み（電源断中の write 失敗）は
// CRC 不一致として検出され「セーブ無し」扱いになる。
struct GameSaveData {
    // 'MOK5'：進化仕様 16 種対応で構造変更（sheep_type 削除、horn/tend_polish 追加）
    static constexpr uint32_t MAGIC = 0x4D4F4B35;

    uint32_t magic;
    char     name[8];                          // 名前 romaji（display 互換）
    uint8_t  name_kana[SAVE_NAME_KANA_LEN];    // 名前ひらがな index 列
    uint8_t  stage;          // Game::Stage の生値
    uint8_t  breed;          // Game::Breed の生値
    uint8_t  hunger;
    uint8_t  happy;
    uint8_t  sleepy;
    uint8_t  wool;
    uint8_t  horn;           // ワイルド系の角の長さ
    uint8_t  sleeping;
    uint8_t  lifespan_days;  // 個体寿命（10-15 日のランダム値）
    uint32_t age_ticks;
    int16_t  tend_feed;
    int16_t  tend_pet;
    int16_t  tend_shear;
    int16_t  tend_polish;    // POLISH（角研ぎ）の世話回数
    uint8_t  grave_count;
    GraveRecord graves[MAX_GRAVES];
    uint32_t crc;            // 上記全フィールドを対象とした CRC32（load 時検証）
};

// フラッシュ末尾の 4KB セクタにデータを保存する。
class Save {
public:
    Save() = default;
    bool load(GameSaveData* out) const;     // 有効なデータがあれば true
    void write(const GameSaveData& data);   // セクタ消去 → プログラム
};
