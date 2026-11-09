/*
 * AETHER - by Synth Centric
 * patches.c : WAVE LAB wavetable templates and transformations, runtime
 * wavetable evolution, and the factory instrument (patch) library.
 *
 * Waves are 32 x 4-bit samples packed two per byte (high nibble first),
 * exactly the layout of the DMG wave RAM.  The project stores the user's
 * waves; the driver plays "live" copies that can evolve during a
 * performance and are re-synchronised from the project on restart.
 * Wave changes reach the hardware only at the next note onset of the wave
 * channel (discrete replacement, never a crossfade).
 */
#pragma bank 255
#include <string.h>
#include "engine.h"
#include "audio.h"

static const uint8_t wtempl[NUM_WTEMPL][16] = {
    {0x89, 0xAC, 0xDE, 0xEF, 0xFF, 0xEE, 0xDC, 0xA9, 0x86, 0x53, 0x21, 0x10, 0x00, 0x11, 0x23, 0x56}, /* SINE */
    {0x01, 0x23, 0x45, 0x67, 0x88, 0x9A, 0xBC, 0xDE, 0xFE, 0xDC, 0xBA, 0x98, 0x87, 0x65, 0x43, 0x21}, /* TRI */
    {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}, /* SAW */
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* SQUARE */
    {0x8B, 0xEF, 0xFE, 0xCB, 0xBB, 0xBA, 0xA9, 0x88, 0x87, 0x76, 0x55, 0x44, 0x44, 0x31, 0x00, 0x14}, /* ORGAN */
    {0x8B, 0xEF, 0xFE, 0xEE, 0xEE, 0xEE, 0xFF, 0xEB, 0x84, 0x10, 0x01, 0x11, 0x11, 0x11, 0x00, 0x14}, /* HOLLOW */
    {0x8B, 0xB9, 0xAA, 0xAD, 0xFD, 0xAA, 0xA9, 0xBB, 0x84, 0x46, 0x55, 0x52, 0x02, 0x55, 0x56, 0x44}, /* GLASS */
    {0x8C, 0xFF, 0xDC, 0xCC, 0xBA, 0x98, 0x88, 0x77, 0x88, 0x87, 0x77, 0x65, 0x43, 0x33, 0x20, 0x03}, /* REED */
    {0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* PULSE25 */
    {0x89, 0xA9, 0x9A, 0xEF, 0xC9, 0xAC, 0xDB, 0x88, 0x86, 0x56, 0x65, 0x10, 0x36, 0x53, 0x24, 0x77}, /* FORMANT */
};

uint8_t wave_get(uint8_t slot, uint8_t i) BANKED
{
    uint8_t b = proj.wave[slot & 3][(i >> 1) & 15];
    return (i & 1) ? (uint8_t)(b & 15) : (uint8_t)(b >> 4);
}

static void raw_set(uint8_t *w, uint8_t i, uint8_t v)
{
    uint8_t *b = &w[(i >> 1) & 15];
    v &= 15;
    if (i & 1) *b = (uint8_t)((*b & 0xF0) | v);
    else *b = (uint8_t)((*b & 0x0F) | (v << 4));
}

static uint8_t raw_get(const uint8_t *w, uint8_t i)
{
    uint8_t b = w[(i >> 1) & 15];
    return (i & 1) ? (uint8_t)(b & 15) : (uint8_t)(b >> 4);
}

void wave_sync_live(void) BANKED
{
    memcpy(live_wave, proj.wave, sizeof(live_wave));
    live_wave_ver++;
}

void wave_set(uint8_t slot, uint8_t i, uint8_t v) BANKED
{
    slot &= 3;
    raw_set(proj.wave[slot], i, v);
    raw_set(live_wave[slot], i, v);
    live_wave_ver++;
}

void wave_template(uint8_t slot, uint8_t kind) BANKED
{
    slot &= 3;
    if (kind >= NUM_WTEMPL) kind = 0;
    memcpy(proj.wave[slot], wtempl[kind], 16);
    memcpy(live_wave[slot], wtempl[kind], 16);
    live_wave_ver++;
}

void wave_transform(uint8_t slot, uint8_t op, Rng *r) BANKED
{
    uint8_t s[32], o[32], i, lo = 15, hi = 0;
    uint8_t *w;
    slot &= 3;
    w = proj.wave[slot];
    for (i = 0; i < 32; i++) s[i] = raw_get(w, i);
    for (i = 0; i < 32; i++) o[i] = s[i];
    switch (op) {
    case WX_RANDOM:
        for (i = 0; i < 32; i++) s[i] = (uint8_t)(rng8(r) & 15);
        /* one smoothing pass keeps it musical */
        /* FALLTHRU */
    case WX_SMOOTH:
        for (i = 0; i < 32; i++)
            o[i] = (uint8_t)((s[(i + 31) & 31] + 2 * s[i] + s[(i + 1) & 31] + 2) >> 2);
        break;
    case WX_MUTATE:
        for (i = 0; i < 4; i++) {
            uint8_t k = (uint8_t)(rng8(r) & 31);
            int8_t v = (int8_t)o[k] + rng_spread(r, 3);
            if (v < 0) v = 0;
            if (v > 15) v = 15;
            o[k] = (uint8_t)v;
        }
        break;
    case WX_INVERT:  for (i = 0; i < 32; i++) o[i] = (uint8_t)(15 - s[i]); break;
    case WX_REVERSE: for (i = 0; i < 32; i++) o[i] = s[31 - i]; break;
    case WX_SHIFT:   for (i = 0; i < 32; i++) o[i] = s[(i + 4) & 31]; break;
    case WX_FOLD:
        for (i = 0; i < 32; i++) {
            int8_t v = (int8_t)(((int8_t)s[i] - 8) * 2);
            if (v > 7) v = 14 - v;
            if (v < -8) v = -16 - v;
            if (v > 7) v = 7;
            if (v < -8) v = -8;
            o[i] = (uint8_t)(v + 8);
        }
        break;
    case WX_CRUSH:   for (i = 0; i < 32; i++) o[i] = (uint8_t)((s[i] & 0x0C) | ((s[i] & 0x0C) >> 2)); break;
    case WX_AMP:
        for (i = 0; i < 32; i++) { if (s[i] < lo) lo = s[i]; if (s[i] > hi) hi = s[i]; }
        if (hi > lo)
            for (i = 0; i < 32; i++) o[i] = (uint8_t)(((uint16_t)(s[i] - lo) * 15) / (hi - lo));
        break;
    case WX_SOFTEN:  for (i = 0; i < 32; i++) o[i] = (uint8_t)(4 + (s[i] >> 1)); break;
    default: break;
    }
    for (i = 0; i < 32; i++) raw_set(w, i, o[i]);
    memcpy(live_wave[slot], w, 16);
    live_wave_ver++;
}

/* runtime evolution: bounded random walk of the live copy around the
 * stored wave, so long performances transform the timbre slowly but
 * can never drift into an unrelated sound. */
void wave_evolve(uint8_t slot, uint8_t amount, Rng *r) BANKED
{
    uint8_t n, k, v, home;
    int8_t d;
    slot &= 3;
    for (n = 0; n < amount; n++) {
        k = (uint8_t)(rng8(r) & 31);
        v = raw_get(live_wave[slot], k);
        home = raw_get(proj.wave[slot], k);
        d = (int8_t)v - (int8_t)home;
        if (d > 3 || d < -3 || rng_chance(r, 70)) {
            if (v > home) v--; else if (v < home) v++;
        } else {
            v = (uint8_t)(rng8(r) & 1 ? (v < 15 ? v + 1 : v) : (v ? v - 1 : v));
        }
        raw_set(live_wave[slot], k, v);
    }
    live_wave_ver++;
}

/* ------------------------------------------------- factory patch library */
typedef struct { char name[INST_NAME_LEN + 1]; uint8_t p[IP_COUNT]; } PatchDef;

/*                         TYP SHP VOL EDR EPC LEN VBD VBR VDL ARP ASP PEN PSP SWP SWD SWS RTG RND OCT MOD DEN IRR PATTERN */
static const PatchDef factory[NUM_FACTORY_PATCHES] = {
    {"GLASS ", { 0,  1, 11,  0,  4,  8,  2,  5,  3,  0,  4, 12,  0,  0,  0,  0,  1,  1,  2,  0,  0,  0, 0x00 }},
    {"HALO  ", { 0,  2,  3,  1,  6, 10,  3,  3,  6,  0,  4, 12,  0,  0,  0,  0,  1,  1,  2,  3,  0,  0, 0x00 }},
    {"BELL  ", { 0,  0, 13,  0,  3,  8,  1,  6,  4,  0,  4, 24, 13,  0,  0,  0,  1,  2,  2,  0,  0,  0, 0x00 }},
    {"CHIME ", { 0,  1, 12,  0,  2,  8,  0,  0,  0,  0,  4, 12,  0,  3,  1,  7,  1,  1,  2,  0,  0,  0, 0x00 }},
    {"PAD   ", { 0,  2,  9,  0,  7, 12,  1,  2,  8,  0,  4, 12,  0,  0,  0,  0,  0,  0,  2,  1,  0,  0, 0x00 }},
    {"ARPEG ", { 0,  1, 10,  0,  2,  8,  0,  0,  0,  6,  3, 12,  0,  0,  0,  0,  1,  0,  2,  0,  0,  0, 0x00 }},
    {"MOTE  ", { 0,  0,  7,  0,  1,  4,  0,  0,  0,  0,  4, 12,  0,  0,  0,  0,  1,  4,  2,  0,  0,  0, 0x00 }},
    {"REED  ", { 0,  2,  8,  0,  5,  9,  2,  4,  5,  0,  4, 11, 10,  0,  0,  0,  1,  1,  2,  6,  0,  0, 0x00 }},
    {"DRONE ", { 1,  0,  3,  0,  0, 15,  1,  1,  8,  0,  4, 12,  0,  1,  0,  2,  1,  0,  2,  3,  0,  0, 0x00 }},
    {"WARM  ", { 1,  1,  2,  0,  6, 10,  2,  3,  6,  0,  4, 12,  0,  1,  0,  0,  1,  1,  2,  2,  0,  0, 0x00 }},
    {"SUB   ", { 1,  0,  3,  0,  0, 12,  0,  0,  0,  0,  4, 12,  0,  0,  0,  0,  0,  0,  1,  0,  0,  0, 0x00 }},
    {"CRYSTL", { 1,  2,  2,  0,  3,  8,  1,  7,  2,  0,  4, 12,  0,  3,  0,  4,  1,  0,  2,  6,  0,  0, 0x00 }},
    {"RAIN  ", { 2,  0,  5,  0,  1,  4,  0,  0,  0, 19,  4, 12,  0,  0,  0,  0,  1,  0,  2,  4, 12, 10, 0xFF }},
    {"WIND  ", { 2,  0,  2,  1,  7, 15,  5,  4,  0,  8,  3, 12,  0,  0,  0,  0,  1,  0,  2,  6,  3,  6, 0x11 }},
    {"DUST  ", { 2,  0,  4,  0,  1,  2,  0,  0,  0, 21,  2, 12,  0,  0,  0,  0,  1,  0,  2,  2,  4, 14, 0xA5 }},
    {"STATIC", { 2,  1,  4,  0,  2,  6,  3, 10,  0, 16,  5, 12,  0,  0,  0,  0,  1,  0,  2,  3,  9,  4, 0xEE }},
    {"SPARK ", { 2,  1,  7,  0,  1,  2,  0,  0,  0, 22,  1, 12,  0,  0,  0,  0,  1,  0,  2,  1,  3, 12, 0x81 }},
    {"SWELL ", { 0,  3,  1,  1,  4, 14,  2,  2,  9,  0,  4, 12,  0,  0,  0,  0,  0,  1,  2,  2,  0,  0, 0x00 }},
};

void patch_factory(uint8_t slot, uint8_t idx) BANKED
{
    if (idx >= NUM_FACTORY_PATCHES) idx = 0;
    memcpy(proj.inst[slot & 7].p, factory[idx].p, IP_COUNT);
    memcpy(proj.inst[slot & 7].name, factory[idx].name, INST_NAME_LEN);
}

void patch_factory_name(uint8_t idx, char *out) BANKED
{
    uint8_t i;
    if (idx >= NUM_FACTORY_PATCHES) idx = 0;
    for (i = 0; i < INST_NAME_LEN; i++) out[i] = factory[idx].name[i];
    out[INST_NAME_LEN] = 0;
}
