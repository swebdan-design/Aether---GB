/*
 * AETHER - by Synth Centric
 * host_platform.c : host replacements for the Game Boy-only pieces
 * (hardware audio driver, battery SRAM) used by the automated tests.
 * The "player" below mimics the VBlank driver's queue consumption so the
 * tests can inspect exactly what the hardware would have been asked to do.
 */
#include <string.h>
#include "aether.h"
#include "audio.h"
#include "storage.h"

Project proj;
volatile uint16_t audio_now;
volatile uint8_t ch_meter[NUM_CH];
volatile uint8_t isr_lines;
uint8_t host_sram[32768];

uint16_t audio_time(void) { return audio_now; }
void audio_all_off(void) { aq_tail = aq_head; }
void audio_init(void) { aq_head = aq_tail = 0; audio_now = 0; }
void audio_set_master(uint8_t v) { (void)v; }

void sram_rd(uint16_t addr, uint8_t *dst, uint16_t n) { memcpy(dst, host_sram + addr, n); }
void sram_wr(uint16_t addr, const uint8_t *src, uint16_t n) { memcpy(host_sram + addr, src, n); }
