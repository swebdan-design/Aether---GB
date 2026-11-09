/*
 * AETHER - by Synth Centric
 * modulation.c : 4-slot modulation matrix, evaluated every 4 frames.
 *
 * Sources produce signed values -128..127, depth is -15..+15, outputs per
 * destination are summed and clamped into eng.mod[] (int8).  Destinations
 * are read by the generator when it builds events (pitch offset, register
 * shift, density, wave morph, duty, length, noise pitch, volume, stereo,
 * mutation).  Integer arithmetic only.
 */
#pragma bank 255
#include "engine.h"


static const uint8_t lfo_inc[16] = { 1, 2, 3, 4, 6, 8, 11, 16, 22, 32, 45, 64, 90, 128, 180, 255 };

static int8_t tri8(uint8_t ph)
{
    /* 0..255 -> triangle -128..127 */
    if (ph < 128) return (int8_t)((int16_t)ph * 2 - 128);
    return (int8_t)(127 - (int16_t)(ph - 128) * 2);
}

static int8_t hash8(uint8_t a, uint8_t b)
{
    uint8_t x = (uint8_t)(a * 37 + b * 101 + 17);
    x ^= (uint8_t)(x << 3);
    x ^= (uint8_t)(x >> 5);
    x = (uint8_t)(x * 29 + 71);
    return (int8_t)x;
}

void mod_reset(void) BANKED
{
    uint8_t i;
    for (i = 0; i < NUM_MODS; i++) {
        eng.mrt[i].phase = (uint16_t)i << 13;
        eng.mrt[i].val = 0;
        eng.mrt[i].tgt = 0;
        eng.mrt[i].cnt = 0;
    }
    for (i = 0; i < NUM_MDEST; i++) eng.mod[i] = 0;
}

int8_t mod_source(uint8_t i) BANKED
{
    return eng.mrt[i].val;
}

void mod_update(void) BANKED
{
    uint8_t i;
    int16_t acc[NUM_MDEST];
    for (i = 0; i < NUM_MDEST; i++) acc[i] = 0;

    for (i = 0; i < NUM_MODS; i++) {
        ModSlot *m = &proj.mod[i];
        ModRt *r = &eng.mrt[i];
        uint8_t rate = m->rate & 15;
        int8_t v = 0;
        switch (m->src) {
        case MS_LFO:
            r->phase += lfo_inc[rate];
            v = tri8((uint8_t)(r->phase >> 4));
            break;
        case MS_RWALK:
            if (++r->cnt > (uint8_t)(16 - rate)) {
                int16_t x = (int16_t)r->val + rng_spread(&eng.rng, 12);
                r->cnt = 0;
                if (x > 127) x = 127 - (x - 127);
                if (x < -128) x = -128 - (x + 128);
                v = (int8_t)x;
            } else v = r->val;
            break;
        case MS_SH:
            r->phase += lfo_inc[rate];
            if (r->phase & 0x1000) { r->phase &= 0x0FFF; r->tgt = (int8_t)rng8(&eng.rng); }
            v = r->tgt;
            break;
        case MS_ENV: {
            /* attack/decay envelope retriggered at every scene change */
            uint16_t e = eng.form.elapsed;
            uint16_t att = (uint16_t)(rate + 1) * 4;     /* seconds */
            if (e < att) v = (int8_t)(((uint32_t)e * 255 / att) - 128);
            else if (e < att * 4) v = (int8_t)(127 - (int16_t)(((uint32_t)(e - att) * 255) / (att * 3)));
            else v = -128;
            break; }
        case MS_DRIFT:
            if (++r->cnt > (uint8_t)(64 - rate * 3)) {
                r->cnt = 0;
                r->tgt = (int8_t)rng8(&eng.rng);
            }
            if (r->val < r->tgt) v = (int8_t)(r->val + 1);
            else if (r->val > r->tgt) v = (int8_t)(r->val - 1);
            else v = r->val;
            break;
        case MS_STEP:
            r->phase += lfo_inc[rate];
            v = hash8((uint8_t)((r->phase >> 11) & 7), i);
            break;
        case MS_SCENE: {
            uint16_t len = (uint16_t)eng.form.live.s[SC_LEN] * 15;
            if (!len) len = 15;
            if (eng.form.elapsed >= len) v = 127;
            else v = (int8_t)(((uint32_t)eng.form.elapsed * 255 / len) - 128);
            break; }
        case MS_ENERGY:  v = (int8_t)(ECO8(ECO_ENERGY) - 128); break;
        case MS_TENSION: v = (int8_t)(ECO8(ECO_TENSION) - 128); break;
        case MS_STAB:    v = (int8_t)(ECO8(ECO_STAB) - 128); break;
        case MS_MEMORY:  v = (int8_t)(ECO8(ECO_MEMORY) - 128); break;
        case MS_TIDE:    v = (int8_t)(eng.tide - 128); break;
        default: v = 0; break;
        }
        r->val = v;
        if (m->src != MS_OFF && m->dest < NUM_MDEST) {
            int8_t depth = (int8_t)m->depth - 15;
            acc[m->dest] += (int16_t)(((int16_t)v * depth) >> 4);
        }
    }
    for (i = 0; i < NUM_MDEST; i++) {
        int16_t x = acc[i];
        if (x > 127) x = 127;
        if (x < -128) x = -128;
        eng.mod[i] = (int8_t)x;
    }
}
