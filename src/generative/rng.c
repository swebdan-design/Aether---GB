/*
 * AETHER - by Synth Centric
 * rng.c : deterministic pseudo-random streams (16-bit xorshift, triple
 * 7/9/8).  Period 65535.  Every agent owns an independent stream seeded
 * from the project seed and the agent's SEED parameter, so a fixed project
 * and seed always reproduce the same musical decision sequence.
 * Fixed bank 0: used by every module.
 */
#include "engine.h"

void rng_seed(Rng *r, uint16_t seed) NONBANKED
{
    r->s = seed ? seed : 0xACE1;
}

uint16_t rng16(Rng *r) NONBANKED
{
    uint16_t x = r->s;
    x ^= (uint16_t)(x << 7);
    x ^= (uint16_t)(x >> 9);
    x ^= (uint16_t)(x << 8);
    r->s = x;
    return x;
}

uint8_t rng8(Rng *r) NONBANKED
{
    uint16_t x = rng16(r);
    return (uint8_t)(x ^ (x >> 8));
}

uint8_t rng_below(Rng *r, uint8_t n) NONBANKED
{
    if (n <= 1) return 0;
    return (uint8_t)(((uint16_t)rng8(r) * n) >> 8);
}

uint8_t rng_chance(Rng *r, uint8_t p) NONBANKED
{
    return rng8(r) < p;
}

int8_t rng_spread(Rng *r, uint8_t n) NONBANKED
{
    return (int8_t)((int8_t)rng_below(r, (uint8_t)(2 * n + 1)) - (int8_t)n);
}

uint16_t scale128(uint16_t x, uint8_t f) NONBANKED
{
    if (x < 256) return (uint16_t)((x * f) >> 7);
    if (x < 1024) return (uint16_t)(((x >> 2) * f) >> 5);
    if (x > 4095) x = 4095;
    return (uint16_t)(((x >> 4) * f) >> 3);
}
