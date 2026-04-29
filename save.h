#pragma once
#include <cstdint>

constexpr int MAX_GRAVES = 50;     // FIFO で古い墓から削除

// 死因（お墓メッセージの分岐用）
namespace DeathCauseValue {
    constexpr uint8_t STATUS = 0;   // 餓死 / 不幸死
    constexpr uint8_t AGE    = 1;   // 天寿
}

struct GraveRecord {
    char     name[8];
    char     breed[16];
    uint8_t  age_days;             // 寿命 max 15 なので uint8_t で十分
    uint8_t  death_cause;          // DeathCauseValue::STATUS or AGE
};

// フラッシュに書き込むセーブデータ。サイズが変わったら MAGIC を変えて互換性を切る。
struct GameSaveData {
    static constexpr uint32_t MAGIC = 0x4D4F4B32;  // 'MOK2'（MAX_GRAVES=50 + death_cause 追加で MOK1 から bump）

    uint32_t magic;
    char     name[8];
    uint8_t  stage;          // Game::Stage の生値
    uint8_t  breed;          // Game::Breed の生値
    uint8_t  sheep_type;     // Game::SheepType の生値
    uint8_t  hunger;
    uint8_t  happy;
    uint8_t  sleepy;
    uint8_t  wool;
    uint8_t  sleeping;
    uint8_t  lifespan_days;  // 個体寿命（10-15 日のランダム値）
    uint32_t age_ticks;
    int16_t  tend_feed;
    int16_t  tend_pet;
    int16_t  tend_shear;
    uint8_t  grave_count;
    GraveRecord graves[MAX_GRAVES];
};

// フラッシュ末尾の 4KB セクタにデータを保存する。
class Save {
public:
    Save() = default;
    bool load(GameSaveData* out) const;     // 有効なデータがあれば true
    void write(const GameSaveData& data);   // セクタ消去 → プログラム
};
