/*
 * AETHER - by Synth Centric
 * agents.c : the eight logical musical Agents.
 *
 *  ORBIT   repeating figure around a tonal centre, slowly varied
 *  BLOOM   motif -> grown phrase -> contraction -> return / mutation
 *  DRIFT   constrained random walk in scale-degree space
 *  SCATTER sparse irregular particles, clusters and silences
 *  TIDE    long-period activity level (density, register, duration,
 *          tension) + optional protected drones
 *  ECHO    compositional echo: recalls recent phrases, displaced/varied
 *  ERODE   dissolves material: deletion, silence, motif holes,
 *          harmonic simplification, energy drain (cyclic)
 *  MEMORY  interval Markov table learnt from all agents + motif library
 *
 * Agents never own hardware channels: they emit note requests which the
 * voice allocator places on one of the four DMG channels (or drops).
 * Each agent reads the ecology and writes impulses into it.
 */
#pragma bank 255
#include <string.h>
#include "engine.h"

#define CFG(i) (proj.agent[(i)].a)

static const int8_t orbit_sets[8][6] = {
    {0, 7, 12, -5, -12, 19},   /* FIFTHS  */
    {0, 4, 7, 12, -5, 16},     /* TRIAD   */
    {0, 5, 10, -7, 12, 17},    /* FOURTHS */
    {0, 12, -12, 24, 0, 12},   /* OCTAVES */
    {0, 2, -2, 4, -1, 5},      /* STEPS   */
    {0, 1, 2, -1, 3, -2},      /* CLUSTER */
    {0, 7, 14, -10, 19, -5},   /* WIDE    */
    {0, 2, 5, 7, 9, -3}        /* PENTA   */
};

/* DENSITY x(D+6)/14 and SPACE x(40-S)/40 as /128 fixed-point factors */
static const uint8_t dens_fac[16] = { 55, 64, 73, 82, 91, 101, 110, 119, 128, 137, 146, 155, 165, 174, 183, 192 };
static const uint8_t space_fac[16] = { 128, 125, 122, 118, 115, 112, 109, 106, 102, 99, 96, 93, 90, 86, 83, 80 };

/* ------------------------------------------------------------ helpers */
static uint8_t in_scene(uint8_t i)
{
    return (eng.form.live.s[SC_MASK] >> i) & 1;
}

static void range_of(uint8_t i, uint8_t *lo, uint8_t *hi)
{
    int16_t l = CFG(i)[AP_LO], h = CFG(i)[AP_HI];
    int8_t sh = (int8_t)(eng.mod[MD_NOTESEL] / 16);
    if (CFG(AG_TIDE)[AP_EN] && i != AG_TIDE) sh += (int8_t)(((int16_t)eng.tide - 128) / 40);
    l += sh; h += sh;
    if (l < 24) l = 24;
    if (h > 107) h = 107;
    if (h < l + 2) h = l + 2;
    *lo = (uint8_t)l; *hi = (uint8_t)h;
}

static uint8_t dur4_for(uint8_t i, uint8_t idx)
{
    uint16_t d = dur_frames(idx);
    int8_t s = ECO_INFL(ECO_STAB, CFG(i)[AP_ECO]);           /* -64..64 */
    d = scale128(d, (uint8_t)(128 + s));
    if (CFG(AG_TIDE)[AP_EN] && i != AG_TIDE)
        d = scale128(d, (uint8_t)((384 - eng.tide) >> 1));     /* high tide: shorter */
    d >>= 2;
    if (d < 1) d = 1;
    if (d > 255) d = 255;
    return (uint8_t)d;
}

static uint8_t pick_dur_idx(uint8_t i, Rng *r)
{
    uint8_t a = CFG(i)[AP_DMIN] & 15, b = CFG(i)[AP_DMAX] & 15;
    if (b < a) { uint8_t t = a; a = b; b = t; }
    return (uint8_t)(a + rng_below(r, (uint8_t)(b - a + 1)));
}

static uint8_t fire_ok(uint8_t i, Rng *r)
{
    int16_t p = (int16_t)scale128((uint16_t)(CFG(i)[AP_ACT] * 16), dens_fac[proj.macro[MAC_DENSITY] & 15]);
    p = (int16_t)scale128((uint16_t)p, space_fac[proj.macro[MAC_SPACE] & 15]);
    p += ECO_INFL(ECO_ENERGY, CFG(i)[AP_ECO]) / 2;
    if (CFG(AG_TIDE)[AP_EN] && i != AG_TIDE)
        p = (int16_t)scale128((uint16_t)p, (uint8_t)((eng.tide + 128) >> 1));
    p += eng.mod[MD_DENSITY] / 2;
    if (p < 0) p = 0;
    if (p > 250) p = 250;
    return rng_chance(r, (uint8_t)p);
}

static uint8_t mut_prob(uint8_t i)
{
    int16_t p = (int16_t)CFG(i)[AP_MUT] * 8 + (int16_t)proj.macro[MAC_EVOLUTION] * 5 + eng.mod[MD_MUTATE] / 2;
    p += ECO_INFL(ECO_TENSION, CFG(i)[AP_ECO]) / 2;
    p -= ECO_INFL(ECO_STAB, CFG(i)[AP_ECO]) / 2;
    if (p < 0) p = 0;
    if (p > 240) p = 240;
    return (uint8_t)p;
}

static uint8_t adherence(uint8_t i)
{
    int16_t h = CFG(i)[AP_HARM];
    if (eng.eco.resolving) h += 4;
    h -= ECO_INFL(ECO_TENSION, CFG(i)[AP_ECO]) / 24;
    if (h < 0) h = 0;
    if (h > 15) h = 15;
    return (uint8_t)h;
}

/* play a note through the allocator, falling back to the second instrument */
static uint8_t out_note(uint8_t i, uint8_t note, uint8_t d4, uint8_t flags)
{
    uint16_t dropped = eng.st_dropped;
    uint8_t ch = emit_note(i, CFG(i)[AP_INST], note, d4, CFG(i)[AP_VOL], CFG(i)[AP_PRI], flags);
    if (ch == 0xFF && eng.st_dropped != dropped && CFG(i)[AP_INST2] < NUM_INST)
        ch = emit_note(i, CFG(i)[AP_INST2], note, d4, CFG(i)[AP_VOL], CFG(i)[AP_PRI], flags | EMF_NOERODE);
    return ch;
}

static Motif *source_motif(uint8_t i)
{
    uint8_t k;
    for (k = 0; k < NUM_MOTIFS; k++)
        if (proj.motif[k].len && proj.motif[k].source == i) return &proj.motif[k];
    return 0;
}

/* --------------------------------------------------------------- ORBIT */
static int8_t orbit_pick(uint8_t i, Rng *r)
{
    uint8_t radius = CFG(i)[AP_P1] ? CFG(i)[AP_P1] : 1;
    int8_t v = orbit_sets[CFG(i)[AP_P2] & 7][rng_below(r, 6)];
    while (v > (int8_t)radius && v >= 12) v -= 12;
    while (v < -(int8_t)radius && v <= -12) v += 12;
    if (v > (int8_t)radius || v < -(int8_t)radius) v = 0;
    return v;
}

static void orbit_init(uint8_t i)
{
    AgentState *s = &eng.ag[i];
    Motif *m = source_motif(i);
    uint8_t k, lo, hi;
    s->len = CFG(i)[AP_P4];
    if (s->len < 2) s->len = 2;
    if (s->len > 8) s->len = 8;
    if (m) {
        s->len = m->len;
        for (k = 0; k < m->len; k++) s->pat[k] = m->n[k].rel;
    } else {
        for (k = 0; k < s->len; k++) s->pat[k] = orbit_pick(i, &s->rng);
        s->pat[0] = 0;
    }
    range_of(i, &lo, &hi);
    s->centre = harm_centre(lo, hi);
}

static void orbit_fire(uint8_t i)
{
    AgentState *s = &eng.ag[i];
    uint8_t lo, hi, note;
    int8_t off;
    range_of(i, &lo, &hi);
    if (s->pos == 0) s->centre = harm_centre(lo, hi);
    off = s->pat[s->pos];
    if (off != REST && fire_ok(i, &s->rng)) {
        int16_t t = (int16_t)s->centre + off;
        uint8_t rad = ECO8(ECO_TENSION) > 170 ? 2 : 1;
        if (t < lo) t += 12;
        if (t > hi) t -= 12;
        note = harm_choose((uint8_t)t, lo, hi, s->last, adherence(i), rad, &s->rng);
        out_note(i, note, dur4_for(i, pick_dur_idx(i, &s->rng)), 0);
        if ((note % 12) == eng.hs.root) ECO_IMPULSE(ECO_STAB, (int16_t)(2 + (CFG(i)[AP_ECO] >> 2)));
    }
    if (++s->pos >= s->len) {
        s->pos = 0;
        s->cycles++;
        /* gradual variation: replace one element (rests regrow here too) */
        if (rng_chance(&s->rng, (uint8_t)(CFG(i)[AP_P3] * 10 + (mut_prob(i) >> 1)))) {
            uint8_t k = (uint8_t)(1 + rng_below(&s->rng, (uint8_t)(s->len - 1)));
            if (ECO8(ECO_TENSION) > 180 && rng_chance(&s->rng, 100))
                s->pat[k] = rng_spread(&s->rng, CFG(i)[AP_P1] ? CFG(i)[AP_P1] : 1);
            else s->pat[k] = orbit_pick(i, &s->rng);
        }
    }
}

/* --------------------------------------------------------------- BLOOM */
static void bloom_seed(uint8_t i)
{
    AgentState *s = &eng.ag[i];
    Motif *m = source_motif(i);
    uint8_t k;
    if (m) {
        s->mlen = m->len > 8 ? 8 : m->len;
        for (k = 0; k < s->mlen; k++) {
            s->motif[k] = (int8_t)((m->n[k].rel * 7) / 12);          /* semitones -> degrees */
            s->mdur[k] = (uint8_t)(m->n[k].dur > 48 ? 9 : m->n[k].dur > 16 ? 7 : m->n[k].dur > 6 ? 5 : 3);
        }
        return;
    }
    s->mlen = CFG(i)[AP_P1];
    if (s->mlen < 2) s->mlen = 2;
    if (s->mlen > 6) s->mlen = 6;
    s->motif[0] = 0;
    s->mdur[0] = pick_dur_idx(i, &s->rng);
    for (k = 1; k < s->mlen; k++) {
        s->motif[k] = (int8_t)(s->motif[k - 1] + rng_spread(&s->rng, 2));
        s->mdur[k] = pick_dur_idx(i, &s->rng);
    }
}

static void bloom_restate(uint8_t i)
{
    AgentState *s = &eng.ag[i];
    uint8_t k;
    for (k = 0; k < s->mlen; k++) { s->pat[k] = s->motif[k]; s->pdur[k] = s->mdur[k]; }
    s->len = s->mlen;
    s->pos = 0;
    s->phase = 0;
}

static void bloom_init(uint8_t i)
{
    bloom_seed(i);
    bloom_restate(i);
}

static void bloom_develop(uint8_t i)
{
    AgentState *s = &eng.ag[i];
    Rng *r = &s->rng;
    uint8_t maxlen = CFG(i)[AP_P4];
    uint8_t k;
    if (maxlen < 4) maxlen = 4;
    if (maxlen > 16) maxlen = 16;
    s->cycles++;
    if (s->phase == 0) {
        /* GROW: append a varied copy of the motif */
        if (s->len + s->mlen <= maxlen) {
            int8_t anchor = s->pat[s->len - 1] == REST ? 0 : s->pat[s->len - 1];
            int8_t shift = rng_spread(r, 3);
            uint8_t branch = rng_chance(r, (uint8_t)(CFG(i)[AP_P2] * 12 + (ECO_INFL(ECO_TENSION, CFG(i)[AP_ECO]) > 0 ? 40 : 0)));
            uint8_t retro = branch && (rng8(r) & 1);
            for (k = 0; k < s->mlen; k++) {
                uint8_t src = retro ? (uint8_t)(s->mlen - 1 - k) : k;
                int8_t d = (int8_t)(s->motif[src] - s->motif[0]);
                if (branch && !retro) d = (int8_t)-d;              /* inversion */
                if (rng_chance(r, (uint8_t)(CFG(i)[AP_P3] * 10 + proj.macro[MAC_SPACE] * 4)))
                    s->pat[s->len + k] = REST;                    /* silence insertion */
                else s->pat[s->len + k] = (int8_t)(anchor + shift + d);
                s->pdur[s->len + k] = s->mdur[src];
            }
            s->len += s->mlen;
            ECO_IMPULSE(ECO_ENERGY, 4);
            if (branch) ECO_IMPULSE(ECO_TENSION, 6);
        } else s->phase = 1;
    } else {
        /* CONTRACT: drop elements from the front */
        uint8_t drop = (uint8_t)(s->mlen > 2 ? s->mlen >> 1 : 1);
        if (s->len - drop < s->mlen) drop = (uint8_t)(s->len - s->mlen);
        for (k = 0; k + drop < s->len; k++) { s->pat[k] = s->pat[k + drop]; s->pdur[k] = s->pdur[k + drop]; }
        s->len -= drop;
        ECO_IMPULSE(ECO_ENERGY, -3);
        if (s->len <= s->mlen) {
            /* return to the origin motif, or let it mutate */
            int16_t pr = 90 + ECO_INFL(ECO_MEMORY, CFG(i)[AP_ECO]) * 2;
            if (pr < 0) pr = 0;
            if (rng_chance(r, (uint8_t)(pr > 250 ? 250 : pr))) {
                ECO_IMPULSE(ECO_MEMORY, 10);
            } else if (rng_chance(r, mut_prob(i) + 40)) {
                k = (uint8_t)(1 + rng_below(r, (uint8_t)(s->mlen - 1)));
                s->motif[k] = (int8_t)(s->motif[k] + rng_spread(r, 2));
                if (rng_chance(r, 60)) s->mdur[k] = pick_dur_idx(i, r);
            }
            bloom_restate(i);
        }
    }
}

static void bloom_fire(uint8_t i)
{
    AgentState *s = &eng.ag[i];
    uint8_t lo, hi;
    if (s->pos < s->len) {
        int8_t el = s->pat[s->pos];
        if (el != REST && fire_ok(i, &s->rng)) {
            uint8_t c, n;
            range_of(i, &lo, &hi);
            c = harm_centre(lo, hi);
            n = harm_degree(c, el);
            while (n > hi) n -= 12;
            while (n < lo) n += 12;
            n = harm_choose(n, lo, hi, s->last, adherence(i), 1, &s->rng);
            out_note(i, n, dur4_for(i, s->pdur[s->pos]), 0);
        }
        s->pos++;
    }
    if (s->pos >= s->len) {
        s->pos = 0;
        bloom_develop(i);
    }
}

/* --------------------------------------------------------------- DRIFT */
static void drift_init(uint8_t i)
{
    uint8_t lo, hi;
    range_of(i, &lo, &hi);
    eng.ag[i].centre = harm_centre(lo, hi);
    eng.ag[i].pat[0] = (int8_t)((int8_t)(CFG(i)[AP_P4] > 14 ? 14 : CFG(i)[AP_P4]) - 7);   /* starting note */
}

static void drift_fire(uint8_t i)
{
    AgentState *s = &eng.ag[i];
    Rng *r = &s->rng;
    uint8_t lo, hi, note;
    int8_t cur = s->pat[0], step, maxs, bias;
    int16_t e;
    if (!fire_ok(i, r)) return;
    range_of(i, &lo, &hi);
    s->centre = harm_centre(lo, hi);
    maxs = (int8_t)(CFG(i)[AP_P1] ? CFG(i)[AP_P1] : 1);
    e = ECO_INFL(ECO_ENERGY, CFG(i)[AP_ECO]);
    if (e > 0) maxs += (int8_t)(e >> 5);
    maxs += (int8_t)(proj.macro[MAC_MOTION] / 6);
    if (maxs > 7) maxs = 7;
    step = rng_spread(r, (uint8_t)maxs);
    bias = (int8_t)CFG(i)[AP_P2] - 7;
    if (bias && rng_below(r, 16) < (uint8_t)((bias > 0 ? bias : -bias) * 2)) step += bias > 0 ? 1 : -1;
    {
        int16_t pr = (int16_t)CFG(i)[AP_P3] * 12 + ECO_INFL(ECO_STAB, CFG(i)[AP_ECO]);
        if (pr > 0 && rng_chance(r, (uint8_t)(pr > 250 ? 250 : pr)) && cur) {
            int8_t back = cur > 0 ? -cur : (int8_t)-cur;
            if (back > maxs) back = maxs;
            if (back < -maxs) back = (int8_t)-maxs;
            step = back;
        }
    }
    note = harm_degree(s->centre, (int8_t)(cur + step));
    if (note < lo || note > hi) {
        step = (int8_t)-step;
        note = harm_degree(s->centre, (int8_t)(cur + step));
        if (note < lo || note > hi) { step = (int8_t)(cur > 0 ? -1 : 1); note = harm_degree(s->centre, (int8_t)(cur + step)); }
    }
    s->pat[0] = (int8_t)(cur + step);
    if (s->pat[0] > 20) s->pat[0] = 20;
    if (s->pat[0] < -20) s->pat[0] = -20;
    note = harm_choose(note, lo, hi, s->last, adherence(i), (uint8_t)(proj.macro[MAC_MOTION] > 10 ? 2 : 1), r);
    out_note(i, note, dur4_for(i, pick_dur_idx(i, r)), 0);
    if (step > 3 || step < -3) ECO_IMPULSE(ECO_TENSION, 3);
}

/* ------------------------------------------------------------- SCATTER */
static void scatter_fire(uint8_t i)
{
    AgentState *s = &eng.ag[i];
    Rng *r = &s->rng;
    uint8_t lo, hi, note, idx, inst;
    if (rng_chance(r, (uint8_t)(CFG(i)[AP_P2] * 14 + proj.macro[MAC_SPACE] * 4))) { s->cluster = 0; return; }
    if (!fire_ok(i, r)) return;
    range_of(i, &lo, &hi);
    note = (uint8_t)(lo + rng_below(r, (uint8_t)(hi - lo + 1)));
    note = harm_choose(note, lo, hi, 0, adherence(i), 3, r);
    idx = pick_dur_idx(i, r);
    if (CFG(i)[AP_P3]) {
        int8_t v = (int8_t)idx + rng_spread(r, (uint8_t)(CFG(i)[AP_P3] >> 2));
        if (v < 0) v = 0;
        if (v > 15) v = 15;
        idx = (uint8_t)v;
    }
    inst = (uint8_t)((CFG(i)[AP_INST] + rng_below(r, (uint8_t)((CFG(i)[AP_P4] & 3) + 1))) & 7);
    {
        uint16_t dropped = eng.st_dropped;
        uint8_t ch = emit_note(i, inst, note, dur4_for(i, idx), CFG(i)[AP_VOL], CFG(i)[AP_PRI], 0);
        if (ch == 0xFF && dropped != eng.st_dropped && CFG(i)[AP_INST2] < NUM_INST)
            emit_note(i, CFG(i)[AP_INST2], note, dur4_for(i, idx), CFG(i)[AP_VOL], CFG(i)[AP_PRI], EMF_NOERODE);
    }
    /* event clustering */
    if (!s->cluster) {
        int16_t pc = (int16_t)CFG(i)[AP_P1] * 14 + ECO_INFL(ECO_ENERGY, CFG(i)[AP_ECO]);
        if (pc > 0 && rng_chance(r, (uint8_t)(pc > 240 ? 240 : pc))) s->cluster = (uint8_t)(1 + rng_below(r, 4));
    } else s->cluster--;
}

/* ----------------------------------------------------------------- TIDE */
static void tide_second(void)
{
    uint8_t *c = CFG(AG_TIDE);
    uint16_t period;
    int16_t t, tri;
    uint8_t ph;
    if (!c[AP_EN] || !in_scene(AG_TIDE)) {
        if (eng.tide > 128) eng.tide--; else if (eng.tide < 128) eng.tide++;
        return;
    }
    period = (uint16_t)(c[AP_P1] ? c[AP_P1] : 1) * 30;      /* seconds */
    eng.tide_phase += (uint16_t)(65535u / period);
    ph = (uint8_t)(eng.tide_phase >> 8);
    tri = ph < 128 ? (int16_t)ph * 2 - 128 : 127 - (int16_t)(ph - 128) * 2;
    /* occasional new swell target keeps successive cycles different */
    if ((eng.tide_phase & 0x3F00) == 0 || rng_chance(&eng.ag[AG_TIDE].rng, 3))
        eng.tide_tgt = (uint8_t)(64 + rng_below(&eng.ag[AG_TIDE].rng, 128));
    t = 128 + ((tri * (int16_t)c[AP_P2]) >> 4);
    t = (t * 3 + (int16_t)eng.tide_tgt) >> 2;
    if (t < 0) t = 0;
    if (t > 255) t = 255;
    /* slew limit: tides never jump */
    if (t > eng.tide + 4) t = eng.tide + 4;
    if (t < eng.tide - 4) t = eng.tide - 4;
    eng.tide = (uint8_t)t;
    ECO_IMPULSE(ECO_ENERGY, (int16_t)(((int16_t)eng.tide - 128) * c[AP_ECO] >> 8));
    if (eng.tide > 200) ECO_IMPULSE(ECO_TENSION, 2);
}

static void tide_fire(uint8_t i)
{
    AgentState *s = &eng.ag[i];
    uint8_t lo, hi, n;
    if (!rng_chance(&s->rng, (uint8_t)(CFG(i)[AP_P3] * 16))) return;
    /* a drone is still sounding on its channel: let it breathe */
    {
        uint8_t t = proj.inst[CFG(i)[AP_INST] & 7].p[IP_TYPE];
        uint8_t ch = t == IT_WAVE ? CH_WAVE : (t == IT_NOISE ? CH_NOISE : 0xFF);
        if (ch != 0xFF && eng.vc[ch].agent == i && (int16_t)(eng.vc[ch].until - eng.gtime) > 0) return;
    }
    if (!fire_ok(i, &s->rng)) return;
    range_of(i, &lo, &hi);
    n = harm_centre(lo, hi);
    if (rng_chance(&s->rng, 70)) n = harm_degree(n, 4);         /* fifth-ish */
    if (n > hi) n -= 12;
    {
        uint8_t d4 = (uint8_t)(dur_frames(CFG(i)[AP_DMAX] & 15) >> 2);
        out_note(i, n, d4 ? d4 : 1, EMF_PROTECT);
    }
    ECO_IMPULSE(ECO_STAB, 8);
}

/* ----------------------------------------------------------------- ECHO */
static void echo_fire(uint8_t i)
{
    AgentState *s = &eng.ag[i];
    Rng *r = &s->rng;
    MotifNote mn[MOTIF_MAX];
    uint8_t want, got = 0, k, src_agent = 0xFF, first = 0, inst_vol, vol;
    uint16_t last_t = 0;
    int16_t p;
    uint8_t idx[MOTIF_MAX];

    p = (int16_t)CFG(i)[AP_ACT] * 16 + ECO_INFL(ECO_MEMORY, CFG(i)[AP_ECO]) + (int16_t)proj.macro[MAC_SPACE] * 4;
    p = (int16_t)((p * (int16_t)(proj.macro[MAC_DENSITY] + 6)) / 14);
    if (p <= 0 || !rng_chance(r, (uint8_t)(p > 250 ? 250 : p))) return;
    if (eng.hcount < 3) return;

    want = (uint8_t)(3 + rng_below(r, 4));
    /* newest non-echo entry, optionally a few events further back */
    for (k = (uint8_t)rng_below(r, 6); k < eng.hcount; k++) {
        HistEv *h = &eng.hist[HIST_IDX(k)];
        if ((uint16_t)(eng.gtime - h->time) > 2400) break;        /* only recent material */
        if (h->agent < NUM_AGENTS && h->agent != AG_ECHO) { src_agent = h->agent; break; }
    }
    if (src_agent == 0xFF) return;
    /* gather that agent's notes (newest first) */
    for (; k < eng.hcount && got < want; k++) {
        uint8_t hi_ = HIST_IDX(k);
        if (eng.hist[hi_].agent == src_agent) idx[got++] = hi_;
    }
    if (got < 2) return;
    /* chronological order */
    for (k = 0; k < got; k++) {
        HistEv *h = &eng.hist[idx[got - 1 - k]];
        if (k == 0) { first = h->note; mn[0].dt = 0; }
        else {
            uint16_t dt = (uint16_t)((h->time - last_t) >> 2);
            mn[k].dt = (uint8_t)(dt > 255 ? 255 : dt);
        }
        last_t = h->time;
        mn[k].rel = (int8_t)(h->note - first);
        mn[k].dur = h->dur4 ? h->dur4 : 1;
        /* interval modification */
        if (CFG(i)[AP_P4] && rng_chance(r, (uint8_t)(CFG(i)[AP_P4] * 12))) {
            if (rng8(r) & 1) mn[k].rel = (int8_t)-mn[k].rel;
            else mn[k].rel = (int8_t)(mn[k].rel + mn[k].rel / 2);
        }
    }
    /* octave displacement */
    switch (CFG(i)[AP_P2]) {
    case 1: first += 12; break;
    case 2: first -= 12; break;
    case 3: if (rng8(r) & 1) first += 12; else first -= 12; break;
    case 4: first += 7; break;
    default: break;
    }
    inst_vol = proj.inst[CFG(i)[AP_INST] & 7].p[IP_VOL];
    vol = CFG(i)[AP_VOL] ? CFG(i)[AP_VOL] : inst_vol;
    vol = vol > 4 ? (uint8_t)(vol - 3) : 1;                        /* restated softer */
    phrase_start(mn, got, first, i, CFG(i)[AP_INST], vol,
                 (uint8_t)(rng_chance(r, mut_prob(i)) ? 3 : 2),
                 (uint16_t)(rate_frames(CFG(i)[AP_P1]) + rng_below(r, 16)),
                 (uint8_t)(CFG(i)[AP_P3] * 12));
    ECO_IMPULSE(ECO_MEMORY, 8);
}

/* ---------------------------------------------------------------- ERODE */
static void erode_second(void)
{
    uint8_t *c = CFG(AG_ERODE);
    AgentState *s = &eng.ag[AG_ERODE];
    if (!c[AP_EN] || !in_scene(AG_ERODE)) {
        eng.erosion = eng.erosion > 8 ? (uint8_t)(eng.erosion - 8) : 0;
        return;
    }
    if (s->phase == 0) {
        uint16_t mx = (uint16_t)c[AP_P1] * 16;
        uint16_t e = (uint16_t)eng.erosion + (uint16_t)((c[AP_P2] + 1) >> 1);
        if (mx > 240) mx = 240;
        if (e >= mx) {
            e = mx;
            if (c[AP_P4] == 0) s->phase = 1;                      /* dissolution complete */
        }
        eng.erosion = (uint8_t)e;
    } else {
        /* regrowth: material returns */
        eng.erosion = eng.erosion > 6 ? (uint8_t)(eng.erosion - 6) : 0;
        if (!eng.erosion) { s->phase = 0; ECO_IMPULSE(ECO_ENERGY, 30); }
    }
}

static void erode_fire(uint8_t i)
{
    AgentState *s = &eng.ag[i];
    Rng *r = &s->rng;
    uint8_t k;
    if (!rng_chance(r, (uint8_t)(CFG(i)[AP_ACT] * 16))) return;
    ECO_IMPULSE(ECO_ENERGY, (int16_t)-(4 + (CFG(i)[AP_ECO] >> 1)));
    ECO_IMPULSE(ECO_TENSION, -2);
    /* extend silences of the others */
    for (k = 0; k < NUM_AGENTS; k++)
        if (k != i && eng.ag[k].timer) eng.ag[k].timer += (uint16_t)(eng.erosion >> 4);
    /* remove motif components */
    if (rng_chance(r, (uint8_t)(CFG(i)[AP_P3] * 14))) {
        uint8_t t = rng_below(r, 3);
        if (t == 0 && eng.ag[AG_ORBIT].len > 2) {
            uint8_t rests = 0, j, pos;
            for (j = 0; j < eng.ag[AG_ORBIT].len; j++) if (eng.ag[AG_ORBIT].pat[j] == REST) rests++;
            pos = (uint8_t)(1 + rng_below(r, (uint8_t)(eng.ag[AG_ORBIT].len - 1)));
            if (rests + 1 < eng.ag[AG_ORBIT].len) eng.ag[AG_ORBIT].pat[pos] = REST;
        } else if (t == 1 && eng.ag[AG_BLOOM].len > 1) {
            eng.ag[AG_BLOOM].pat[rng_below(r, eng.ag[AG_BLOOM].len)] = REST;
        } else {
            uint8_t row = rng_below(r, MARKOV_N), j;
            for (j = 0; j < MARKOV_N; j++) eng.markov[row][j] = (uint8_t)((eng.markov[row][j] >> 1) + 1);
        }
        ECO_IMPULSE(ECO_MEMORY, -6);
    }
}

/* --------------------------------------------------------------- MEMORY */
void markov_learn(int8_t prev_iv, int8_t iv) BANKED
{
    uint8_t row, col, j;
    if (prev_iv > 6) prev_iv = 6;
    if (prev_iv < -6) prev_iv = -6;
    if (iv > 6) iv = 6;
    if (iv < -6) iv = -6;
    row = (uint8_t)(prev_iv + 6);
    col = (uint8_t)(iv + 6);
    if (eng.markov[row][col] > 240)
        for (j = 0; j < MARKOV_N; j++) eng.markov[row][j] = (uint8_t)((eng.markov[row][j] >> 1) + 1);
    eng.markov[row][col] += 3;
}

static void memory_fire(uint8_t i)
{
    AgentState *s = &eng.ag[i];
    Rng *r = &s->rng;
    uint8_t lo, hi, row, j, note;
    uint16_t total = 0, pick;
    uint8_t sharp = CFG(i)[AP_P2];
    int8_t iv = 0;
    int16_t pr;

    if (!fire_ok(i, r)) return;

    /* library maintenance: periodically remember recent material */
    if (++s->cycles >= (uint8_t)(18 - (CFG(i)[AP_P3] & 15))) {
        s->cycles = 0;
        motif_auto_capture(0xFF);
    }
    /* recall a phrase from the motif library */
    pr = (int16_t)CFG(i)[AP_P1] * 8 + ECO_INFL(ECO_MEMORY, CFG(i)[AP_ECO]);
    if (pr > 0 && rng_chance(r, (uint8_t)(pr > 200 ? 200 : pr))) {
        Motif *m = motif_pick(r, i);
        if (m) { motif_play(m, i, 0); return; }
    }
    /* sample the next interval from the learnt transition table */
    range_of(i, &lo, &hi);
    row = (uint8_t)((s->last_iv > 6 ? 6 : s->last_iv < -6 ? -6 : s->last_iv) + 6);
    for (j = 0; j < MARKOV_N; j++) {
        uint16_t w = eng.markov[row][j];
        if (sharp) w = (uint16_t)((w * w) >> (8 - (sharp >> 2)));     /* sharpen preferences */
        total += w + 1;
    }
    pick = (uint16_t)(((uint32_t)rng16(r) * total) >> 16);
    for (j = 0; j < MARKOV_N; j++) {
        uint16_t w = eng.markov[row][j];
        if (sharp) w = (uint16_t)((w * w) >> (8 - (sharp >> 2)));
        if (pick < w + 1) { iv = (int8_t)(j - 6); break; }
        pick -= w + 1;
    }
    if (!s->last) s->last = harm_centre(lo, hi);
    {
        int16_t t = (int16_t)s->last + iv;
        if (t < lo) t += 12;
        if (t > hi) t -= 12;
        note = harm_choose((uint8_t)t, lo, hi, s->last, adherence(i), 1, r);
    }
    out_note(i, note, dur4_for(i, pick_dur_idx(i, r)), 0);
    ECO_IMPULSE(ECO_MEMORY, 3);
}

/* -------------------------------------------------------------- driver */
static uint16_t next_interval(uint8_t i)
{
    uint8_t *c = CFG(i);
    AgentState *s = &eng.ag[i];
    uint8_t clocked = proj.glob[GP_TMODE] == TM_CLOCKED;
    uint16_t x;
    int8_t e;

    if (i == AG_SCATTER && s->cluster) return clocked ? 1 : (uint16_t)(3 + rng_below(&s->rng, 6));

    x = clocked ? rate_steps(c[AP_RATE]) : rate_frames(c[AP_RATE]);
    x = scale128(x, (uint8_t)((24 - proj.macro[MAC_MOTION]) << 3));     /* MOTION */
    e = ECO_INFL(ECO_ENERGY, c[AP_ECO]);
    x = scale128(x, (uint8_t)(128 - e));                                 /* ENERGY */
    if (CFG(AG_TIDE)[AP_EN] && i != AG_TIDE) x = scale128(x, (uint8_t)((384 - eng.tide) >> 1));
    if (eng.erosion && i != AG_ERODE) x += scale128(x, (uint8_t)(eng.erosion >> 2));
    if (c[AP_JIT]) {
        if (clocked) {
            if (rng_chance(&s->rng, (uint8_t)(c[AP_JIT] * 10))) x += rng_below(&s->rng, (uint8_t)(1 + (c[AP_JIT] >> 2)));
        } else {
            uint16_t j = scale128(x, (uint8_t)(c[AP_JIT] << 3));
            if (j > 250) j = 250;
            x = (uint16_t)(x + rng_below(&s->rng, (uint8_t)(j + 1)) - (j >> 1));
        }
    }
    if (x < 1) x = 1;
    if (x > 4000) x = 4000;
    return (uint16_t)x;
}

void agent_init(uint8_t i) BANKED
{
    AgentState *s = &eng.ag[i];
    uint16_t seed = (uint16_t)(proj.seed_lo | ((uint16_t)proj.seed_hi << 8));
    uint8_t as = CFG(i)[AP_SEED];
    memset(s, 0, sizeof(AgentState));
    rng_seed(&s->rng, (uint16_t)(seed ^ ((uint16_t)as << 8 | as) ^ (uint16_t)(0x1F3D * (i + 1))));
    s->timer = (uint16_t)(1 + rng_below(&s->rng, 40));
    switch (i) {
    case AG_ORBIT: orbit_init(i); break;
    case AG_BLOOM: bloom_init(i); break;
    case AG_DRIFT: drift_init(i); break;
    default: break;
    }
}

void agents_reset(void) BANKED
{
    uint8_t i, j;
    for (i = 0; i < MARKOV_N; i++)
        for (j = 0; j < MARKOV_N; j++) {
            int8_t d = (int8_t)j - 6;
            if (d < 0) d = (int8_t)-d;
            eng.markov[i][j] = (uint8_t)(d <= 5 ? 12 - 2 * d : 1);
        }
    for (i = 0; i < NUM_AGENTS; i++) agent_init(i);
}

void agent_on_scene(uint8_t old_mask, uint8_t new_mask, uint8_t glide) BANKED
{
    uint8_t i;
    for (i = 0; i < NUM_AGENTS; i++) {
        uint8_t b = (uint8_t)(1 << i);
        if ((new_mask & b) && !(old_mask & b))
            eng.ag[i].hold = (uint16_t)rng_below(&eng.ag[i].rng, (uint8_t)(glide * 4 + 1)) * 60;
    }
}

void agents_second(void) BANKED
{
    tide_second();
    erode_second();
}

void agents_tick(void) BANKED
{
    /* hot path (every frame): pointer iteration, no index multiplications */
    uint8_t i, bit = 1;
    uint8_t clocked = proj.glob[GP_TMODE] == TM_CLOCKED;
    uint8_t mask = eng.form.ending ? 0 : eng.form.live.s[SC_MASK];
    AgentState *s = eng.ag;
    const uint8_t *c = proj.agent[0].a;
    for (i = 0; i < NUM_AGENTS; i++, s++, c += AP_COUNT, bit <<= 1) {
        if (s->fired) s->fired--;
        if (!c[AP_EN] || !(mask & bit)) continue;
        if (s->hold) { s->hold--; continue; }
        if (clocked && !eng.step) continue;
        if (s->timer > 1) { s->timer--; continue; }
        switch (i) {
        case AG_ORBIT:   orbit_fire(i); break;
        case AG_BLOOM:   bloom_fire(i); break;
        case AG_DRIFT:   drift_fire(i); break;
        case AG_SCATTER: scatter_fire(i); break;
        case AG_TIDE:    tide_fire(i); break;
        case AG_ECHO:    echo_fire(i); break;
        case AG_ERODE:   erode_fire(i); break;
        default:         memory_fire(i); break;
        }
        s->timer = next_interval(i);
    }
}
