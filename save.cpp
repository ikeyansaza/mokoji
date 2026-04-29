#include "save.h"
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include <cstring>

namespace {
// Pico の 2MB フラッシュ末尾の 1 セクタ（4KB）をセーブ領域に予約。
// コードは先頭から積まれるので、サイズが収まる限り衝突しない。
constexpr uint32_t SAVE_OFFSET = (2u * 1024u * 1024u) - FLASH_SECTOR_SIZE;
}  // namespace

bool Save::load(GameSaveData* out) const {
    auto* p = reinterpret_cast<const GameSaveData*>(XIP_BASE + SAVE_OFFSET);
    if (p->magic != GameSaveData::MAGIC) {
        return false;
    }
    *out = *p;
    return true;
}

void Save::write(const GameSaveData& data) {
    static_assert(sizeof(GameSaveData) <= FLASH_PAGE_SIZE,
                  "GameSaveData が 1 page を超えた。複数 page に拡張する必要がある");

    static uint8_t pageBuf[FLASH_PAGE_SIZE];
    memset(pageBuf, 0xFF, sizeof(pageBuf));
    memcpy(pageBuf, &data, sizeof(data));

    // フラッシュへの erase/program 中は割り込みを止める（XIP がブロックされるため）
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(SAVE_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(SAVE_OFFSET, pageBuf, FLASH_PAGE_SIZE);
    restore_interrupts(ints);
}
