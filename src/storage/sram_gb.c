/*
 * AETHER - by Synth Centric
 * sram_gb.c : MBC5 battery SRAM access (fixed bank 0).
 * RAM is enabled only for the duration of each transfer.
 */
#include <gb/gb.h>
#include <string.h>
#include "storage.h"

void sram_rd(uint16_t addr, uint8_t *dst, uint16_t n) NONBANKED
{
    while (n) {
        uint8_t bank = (uint8_t)(addr >> 13);
        uint16_t off = addr & 0x1FFF;
        uint16_t k = 0x2000 - off;
        if (k > n) k = n;
        ENABLE_RAM;
        SWITCH_RAM(bank);
        memcpy(dst, (const uint8_t *)(0xA000 + off), k);
        DISABLE_RAM;
        dst += k; addr += k; n -= k;
    }
}

void sram_wr(uint16_t addr, const uint8_t *src, uint16_t n) NONBANKED
{
    while (n) {
        uint8_t bank = (uint8_t)(addr >> 13);
        uint16_t off = addr & 0x1FFF;
        uint16_t k = 0x2000 - off;
        if (k > n) k = n;
        ENABLE_RAM;
        SWITCH_RAM(bank);
        memcpy((uint8_t *)(0xA000 + off), src, k);
        DISABLE_RAM;
        src += k; addr += k; n -= k;
    }
}
