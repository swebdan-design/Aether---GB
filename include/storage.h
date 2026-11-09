/*
 * AETHER - by Synth Centric
 * storage.h : battery SRAM project storage.
 *
 * MBC5 + 32 KB SRAM.  8 project slots, each with two redundant copies
 * (A/B, 2 KB each).  A save always goes to the older / invalid copy, so
 * the previous good version survives an interrupted write.
 */
#ifndef AETHER_STORAGE_H
#define AETHER_STORAGE_H
#include "aether.h"

#define NUM_SLOTS   8
#define COPY_SIZE   2048u
#define HDR_SIZE    16u

#define SLOT_EMPTY   0
#define SLOT_OK      1
#define SLOT_CORRUPT 2
#define SLOT_BACKUP  3   /* newest copy damaged, older copy valid */

/* low level, linear SRAM address 0..32767 (implemented per platform) */
void sram_rd(uint16_t addr, uint8_t *dst, uint16_t n) NONBANKED;
void sram_wr(uint16_t addr, const uint8_t *src, uint16_t n) NONBANKED;

uint16_t fletcher16(const uint8_t *d, uint16_t n) BANKED;
uint8_t  save_slot(uint8_t slot) BANKED;          /* 1 ok, 0 failed (verify) */
uint8_t  load_slot(uint8_t slot) BANKED;          /* 1 loaded, 0 nothing valid */
uint8_t  slot_status(uint8_t slot) BANKED;
uint8_t  slot_name(uint8_t slot, char *out) BANKED;  /* 1 if valid */
void     slot_erase(uint8_t slot) BANKED;
#endif
