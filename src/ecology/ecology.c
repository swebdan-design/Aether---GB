/*
 * AETHER - by Synth Centric
 * ecology.c : the Musical Ecology Engine.
 *
 * Four bounded state variables (8.8 fixed point, 0..255.99):
 *     ENERGY   overall activity
 *     TENSION  harmonic / melodic instability
 *     STABILITY continuity, repetition, tonal grounding
 *     MEMORY   influence of previously generated material
 *
 * Every ~16 frames (eco period) the impulses produced by the agents'
 * actions are folded into the variables through a soft limiter, then the
 * variables relax toward their baselines (scene targets + user bias) and
 * interact through a small, damped cross-coupling matrix:
 *     sustained energy      -> raises tension
 *     tension               -> erodes stability
 *     stability             -> feeds memory (repetition becomes memorable)
 *     silence               -> drains energy, slowly restores stability
 *     excessive tension     -> RESOLUTION phase (harmony prefers consonance,
 *                              tension decays faster) until it falls back
 * Variables can never saturate permanently: impulses are scaled by the
 * remaining headroom, and the baseline pull is always active.
 */
#pragma bank 255
#include "engine.h"

void eco_reset(void) BANKED
{
    uint8_t i;
    for (i = 0; i < 4; i++) {
        eng.eco.acc[i] = 0;
        eng.eco.v[i] = (uint16_t)eng.eco.target[i] << 8;
    }
    eng.eco.silent = 0;
    eng.eco.resolving = 0;
    eng.eco.notes = 0;
}

uint8_t eco_get(uint8_t v) BANKED
{
    return (uint8_t)(eng.eco.v[v & 3] >> 8);
}

/* signed influence of a variable on an agent: -64..+64 scaled by the user's
 * influence setting for that variable and by the agent's sensitivity */
int8_t eco_infl(uint8_t v, uint8_t sens) BANKED
{
    int16_t d = (int16_t)(eng.eco.v[v & 3] >> 8) - 128;      /* -128..127 */
    uint8_t k = (uint8_t)(proj.eco[EP_INF_E + (v & 3)] * sens); /* 0..225  */
    return (int8_t)((d * (int16_t)k) >> 9);
}

static uint16_t add_soft(uint16_t v, int16_t d)
{
    /* impulse scaled by remaining headroom -> asymptotic approach to limits */
    int32_t x;
    if (d > 0) {
        uint16_t head = (uint16_t)(65535u - v);
        x = (int32_t)v + (((int32_t)d * (int32_t)(head >> 8)) >> 2);
    } else {
        x = (int32_t)v + (((int32_t)d * (int32_t)(v >> 8)) >> 2);
    }
    if (x < 0) x = 0;
    if (x > 65535) x = 65535;
    return (uint16_t)x;
}

static uint16_t relax(uint16_t v, uint8_t target, uint8_t shift)
{
    int16_t d = (int16_t)target - (int16_t)(v >> 8);  /* in units */
    int32_t x = (int32_t)v + (((int32_t)d << 8) >> shift);
    if (x < 0) x = 0;
    if (x > 65535) x = 65535;
    return (uint16_t)x;
}

void eco_update(void) BANKED
{
    uint8_t i, shift, couple;
    uint8_t E, T, S;
    int16_t c;

    if (proj.eco[EP_FREEZE]) {
        for (i = 0; i < 4; i++) eng.eco.acc[i] = 0;
        eng.eco.notes = 0;
        return;
    }

    /* silence drains energy and lets stability recover */
    if (eng.eco.notes == 0) {
        if (eng.eco.silent < 255) eng.eco.silent++;
        eng.eco.acc[ECO_ENERGY] -= 6;
        if (eng.eco.silent > 8) eng.eco.acc[ECO_STAB] += 2;
    } else eng.eco.silent = 0;
    eng.eco.notes = 0;

    /* cross coupling (damped) */
    couple = proj.eco[EP_COUPLE];
    E = (uint8_t)(eng.eco.v[0] >> 8);
    T = (uint8_t)(eng.eco.v[1] >> 8);
    S = (uint8_t)(eng.eco.v[2] >> 8);
    if (couple) {
        if (E > 150) eng.eco.acc[ECO_TENSION] += (int16_t)(((E - 150) * couple) >> 6);
        if (T > 110) eng.eco.acc[ECO_STAB] -= (int16_t)(((T - 110) * couple) >> 6);
        if (S > 150) eng.eco.acc[ECO_MEMORY] += (int16_t)(((S - 150) * couple) >> 6);
        if (S > 170) eng.eco.acc[ECO_ENERGY] -= (int16_t)(((S - 170) * couple) >> 7);
    }

    /* tension resolution with hysteresis */
    if (!eng.eco.resolving && T > 205) eng.eco.resolving = 1;
    else if (eng.eco.resolving && T < 120) {
        eng.eco.resolving = 0;
        eng.eco.acc[ECO_STAB] += 30;           /* arrival feels grounded */
    }
    if (eng.eco.resolving) eng.eco.acc[ECO_TENSION] -= 12;

    /* EVOLUTION macro and damping set how quickly baselines pull */
    shift = (uint8_t)(7 - (proj.eco[EP_DAMP] >> 2) - (proj.macro[MAC_EVOLUTION] >> 3));
    if (shift < 2) shift = 2;

    for (i = 0; i < 4; i++) {
        c = eng.eco.acc[i];
        if (c > 400) c = 400;
        if (c < -400) c = -400;
        eng.eco.v[i] = add_soft(eng.eco.v[i], c);
        eng.eco.v[i] = relax(eng.eco.v[i], eng.eco.target[i], shift);
        eng.eco.acc[i] = 0;
    }
}
