/*
 * AETHER - by Synth Centric
 * harmony.c : global harmonic framework.
 *
 *  - scale / pitch-class collections (incl. user defined)
 *  - current chord (triad on a scale degree) moving at meso time
 *  - context sensitive pitch choice: every candidate note receives an
 *    additive "log score" (distance to the agent's intention, consonance
 *    against the current chord, interval weighting, harmonic memory,
 *    register separation, melodic motion) that is turned into a weight
 *    through an exponential table, then sampled.  No floating point.
 *  - harmonic memory: decaying pitch-class / interval-class histograms,
 *    recent pitches and tonal-centre history (REINFORCE/EXPLORE/DRIFT).
 */
#pragma bank 255
#include "engine.h"


static const uint16_t scale_tab[NUM_SCALES - 1] = {
    0xAB5, /* major          */
    0x5AD, /* natural minor  */
    0x295, /* major pent.    */
    0x4A9, /* minor pent.    */
    0x6AD, /* dorian         */
    0x5AB, /* phrygian       */
    0xAD5, /* lydian         */
    0x6B5, /* mixolydian     */
    0xFFF  /* chromatic      */
};

/* exp-like weight table: w = 2^(score/5.5) / 2 */
static const uint16_t expw[64] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 4, 4, 5, 5, 6, 7, 8, 9,
    10, 11, 13, 15, 17, 19, 22, 25, 28, 32, 36, 41, 46, 53, 60, 68, 77, 87,
    99, 113, 128, 145, 164, 187, 212, 240, 272, 309, 351, 398, 451, 512,
    581, 659, 747, 847, 961, 1090, 1237, 1403
};

static uint16_t rot12(uint16_t m, uint8_t r)
{
    r %= 12;
    return (uint16_t)(((m << r) | (m >> (12 - r))) & 0xFFF);
}

uint16_t scale_mask(uint8_t scale) BANKED
{
    uint16_t m;
    if (scale >= SCALE_USER) {
        m = (uint16_t)(proj.harm[HP_UMASKL] | ((uint16_t)(proj.harm[HP_UMASKH] & 15) << 8));
        if (!m) m = 1;
        return m;
    }
    return scale_tab[scale];
}

/* list the pitch classes (relative) of a mask in ascending order */
static uint8_t mask_degrees(uint16_t m, uint8_t *out)
{
    uint8_t i, n = 0;
    for (i = 0; i < 12; i++) if (m & (1u << i)) out[n++] = i;
    return n;
}

static void build_chord(void)
{
    uint8_t deg[12], n, d;
    uint16_t rel = scale_mask(eng.hs.scale);
    if (rel == 0xFFF) rel = 0xAB5;              /* chromatic: diatonic chords */
    n = mask_degrees(rel, deg);
    if (n < 3) { eng.hs.chord = rot12(rel, eng.hs.root); return; }
    d = (uint8_t)(eng.hs.degree % n);
    eng.hs.chord = (uint16_t)((1u << deg[d]) | (1u << deg[(d + 2) % n]) | (1u << deg[(d + 4) % n]));
    eng.hs.chord = rot12(eng.hs.chord, eng.hs.root);
}

static uint8_t chord_root_pc(void)
{
    uint8_t deg[12], n;
    uint16_t rel = scale_mask(eng.hs.scale);
    if (rel == 0xFFF) rel = 0xAB5;
    n = mask_degrees(rel, deg);
    return (uint8_t)((deg[eng.hs.degree % n] + eng.hs.root) % 12);
}

static void build_cache(void);

static void build_mask(void)
{
    uint16_t m = rot12(scale_mask(eng.hs.scale), eng.hs.root);
    uint16_t keep;
    uint8_t r = eng.hs.root;
    switch (eng.hs.simplify) {
    case 0: break;
    case 1: keep = (uint16_t)(eng.hs.chord | rot12(0x085, r)); m &= keep; break;  /* chord + 1,3?,5 */
    case 2: m &= (uint16_t)(eng.hs.chord | (1u << r)); break;
    default: m = (uint16_t)((1u << r) | (1u << ((r + 7) % 12))); break;
    }
    if (!m) m = (uint16_t)(1u << r);
    eng.hs.mask = m;
    build_cache();
}

/* pre-computed per pitch class tables: the per-note code then needs no
 * modulo, no variable shifts and no chord recomputation */
static void build_cache(void)
{
    uint8_t pc, cr = chord_root_pc();
    uint16_t m = eng.hs.mask, ch = eng.hs.chord;
    eng.hs.croot = cr;
    for (pc = 0; pc < 12; pc++) {
        eng.hs.pc_ok[pc] = (uint8_t)(m & 1);
        if (pc == cr) eng.hs.pc_cons[pc] = 15;
        else if (ch & 1) eng.hs.pc_cons[pc] = 13;
        else eng.hs.pc_cons[pc] = proj.harm[HP_IW + (uint8_t)((pc + 12 - cr) % 12)] & 15;
        m >>= 1; ch >>= 1;
    }
}

void harm_refresh(void) BANKED
{
    eng.hs.home = proj.harm[HP_ROOT] % 12;
    eng.hs.root = eng.hs.home;
    eng.hs.scale = proj.harm[HP_SCALE] < NUM_SCALES ? proj.harm[HP_SCALE] : 0;
    build_chord();
    build_mask();
}

void harm_reset(void) BANKED
{
    uint8_t i;
    for (i = 0; i < 12; i++) { eng.hs.pc_hist[i] = 0; eng.hs.iv_hist[i] = 0; }
    for (i = 0; i < 16; i++) eng.hs.recent[i] = 0;
    for (i = 0; i < 4; i++) eng.hs.centres[i] = proj.harm[HP_ROOT] % 12;
    eng.hs.rpos = 0;
    eng.hs.degree = 0;
    eng.hs.bars = 0;
    eng.hs.simplify = 0;
    eng.hs.changes = 0;
    harm_refresh();
}

uint8_t harm_consonance(uint8_t note) BANKED
{
    return eng.hs.pc_cons[note % 12];
}

static uint8_t in_mask(uint8_t note)
{
    return eng.hs.pc_ok[note % 12];
}

uint8_t harm_quantize(uint8_t note) BANKED
{
    uint8_t d;
    for (d = 0; d < 7; d++) {
        if (note >= d && in_mask((uint8_t)(note - d))) return (uint8_t)(note - d);
        if (in_mask((uint8_t)(note + d))) return (uint8_t)(note + d);
    }
    return note;
}

uint8_t harm_degree(uint8_t from, int8_t steps) BANKED
{
    uint8_t n = harm_quantize(from);
    while (steps > 0) {
        do { n++; } while (!in_mask(n) && n < 120);
        steps--;
    }
    while (steps < 0) {
        do { n--; } while (!in_mask(n) && n > 12);
        steps++;
    }
    return n;
}

uint8_t harm_centre(uint8_t lo, uint8_t hi) BANKED
{
    uint8_t mid = (uint8_t)((lo + hi) >> 1);
    uint8_t n = (uint8_t)(mid - ((mid + 12 - eng.hs.root) % 12));
    if ((uint8_t)(mid - n) > 6 && (uint8_t)(n + 12) <= hi) n += 12;
    if (n < lo) n += 12;
    return n;
}

uint8_t harm_choose(uint8_t target, uint8_t lo, uint8_t hi, uint8_t prev,
                    uint8_t adherence, uint8_t radius, Rng *r) BANKED
{
    uint16_t w[25];
    uint16_t total = 0, pick;
    uint8_t n, start, end, i, count;
    uint8_t tension = ECO8(ECO_TENSION);
    uint8_t memv = ECO8(ECO_MEMORY);
    uint8_t stab = ECO8(ECO_STAB);
    uint8_t diss, cpref, mstr, cr, regsep, pc;
    uint8_t cmul[16];
    uint8_t mmode = proj.harm[HP_MMODE];
    uint8_t harmac = proj.macro[MAC_HARMONY];

    if (lo < proj.harm[HP_LO]) lo = proj.harm[HP_LO];
    if (hi > proj.harm[HP_HI]) hi = proj.harm[HP_HI];
    if (lo > hi) { uint8_t t = lo; lo = hi; hi = t; }
    if (target < lo) target = lo;
    if (target > hi) target = hi;
    if (radius < 1) radius = 1;
    if (radius > 12) radius = 12;
    start = (target >= lo + radius) ? (uint8_t)(target - radius) : lo;
    end = (uint8_t)(target + radius);
    if (end > hi) end = hi;

    /* dissonance: user probability + HARMONY macro + ecological tension */
    {
        uint16_t pd = (uint16_t)proj.harm[HP_DISS] * 10 + (uint16_t)harmac * 4 +
                      (uint16_t)((tension * proj.eco[EP_INF_T]) >> 6);
        if (eng.eco.resolving) pd >>= 2;
        diss = pd > 255 ? 1 : rng_chance(r, (uint8_t)pd);
    }
    cpref = proj.harm[HP_CONS];
    if (eng.eco.resolving) cpref = (uint8_t)(cpref + (proj.harm[HP_RESOL] >> 1));
    if (cpref > 15) cpref = 15;
    {
        /* consonance x preference products, built by addition */
        uint8_t k, acc = 0;
        for (k = 0; k < 16; k++) { cmul[k] = acc; acc = (uint8_t)(acc + cpref); }
    }
    mstr = (uint8_t)(proj.harm[HP_MSTR] + ((memv * proj.eco[EP_INF_M]) >> 9));
    if (mstr > 15) mstr = 15;
    cr = eng.hs.croot;
    regsep = proj.harm[HP_REGSEP];
    (void)cr;
    {
        /* notes sounding on the other channels, for register separation */
        uint8_t i2, ns = 0;
        uint8_t snd[3];
        if (regsep)
            for (i2 = 0; i2 < NUM_CH - 1; i2++)
                if ((int16_t)(eng.vc[i2].until - eng.gtime) > 0) snd[ns++] = eng.vc[i2].note;

        count = 0;
        pc = start % 12;
        for (n = start; n <= end && count < 25; n++, count++, pc = (uint8_t)(pc == 11 ? 0 : pc + 1)) {
            int16_t sc = 34;
            uint8_t cons, d;
            if (!eng.hs.pc_ok[pc]) {
                if (adherence >= 15) { w[count] = 0; continue; }
                sc -= (int16_t)(adherence * 2) + 4;
            }
            d = n > target ? (uint8_t)(n - target) : (uint8_t)(target - n);
            sc -= (int16_t)(d * 2);
            cons = eng.hs.pc_cons[pc];
            if (!diss) sc += (int16_t)(cmul[cons] >> 3);
            else sc += (int16_t)(((15 - cons) * 12) >> 3);
            if (mstr) {
                uint8_t h = (uint8_t)(eng.hs.pc_hist[pc] >> 4);       /* 0..15 */
                uint8_t hm = (uint8_t)((h * mstr) >> 4);              /* 0..14 */
                if (mmode == MM_REINFORCE) sc += hm;
                else if (mmode == MM_EXPLORE) sc -= hm;
                else {
                    uint8_t p7 = pc >= 5 ? (uint8_t)(pc - 5) : (uint8_t)(pc + 7);
                    uint8_t p5 = pc >= 7 ? (uint8_t)(pc - 7) : (uint8_t)(pc + 5);
                    uint8_t nb = (uint8_t)((eng.hs.pc_hist[p7] >> 5) + (eng.hs.pc_hist[p5] >> 5));
                    sc += (int16_t)((((int16_t)nb - (int16_t)h) * (int16_t)mstr) >> 4);
                }
            }
            if (prev) {
                uint8_t iv = n > prev ? (uint8_t)(n - prev) : (uint8_t)(prev - n);
                if (iv == 0) sc -= (int16_t)(6 - (stab >> 6));
                else if (iv > 7) sc -= (int16_t)((iv - 7) * 2);
                sc += (int16_t)(proj.harm[HP_IW + (iv < 12 ? iv : iv - 12 * (iv / 12))] >> 2);
            }
            for (i2 = 0; i2 < ns; i2++) {
                uint8_t o = snd[i2];
                uint8_t dd = n > o ? (uint8_t)(n - o) : (uint8_t)(o - n);
                if (dd == 0) sc -= 5;
                else if (dd < regsep) sc -= 9;
            }
            if (sc < 0) sc = 0;
            if (sc > 63) sc = 63;
            w[count] = expw[(uint8_t)sc];
            total += w[count];
        }
    }

    if (total == 0) return harm_quantize(target);
    pick = (uint16_t)(((uint32_t)rng16(r) * total) >> 16);
    for (i = 0; i < count; i++) {
        if (pick < w[i]) return (uint8_t)(start + i);
        pick -= w[i];
    }
    return (uint8_t)(start + count - 1);
}

void harm_note(uint8_t note, uint8_t prev) BANKED
{
    uint8_t pc = note % 12, i;
    for (i = 0; i < 12; i++) {
        eng.hs.pc_hist[i] -= eng.hs.pc_hist[i] >> 5;
        eng.hs.iv_hist[i] -= eng.hs.iv_hist[i] >> 5;
    }
    eng.hs.pc_hist[pc] = eng.hs.pc_hist[pc] > 231 ? 255 : (uint8_t)(eng.hs.pc_hist[pc] + 24);
    if (prev) {
        uint8_t iv = (uint8_t)((note > prev ? note - prev : prev - note) % 12);
        eng.hs.iv_hist[iv] = eng.hs.iv_hist[iv] > 231 ? 255 : (uint8_t)(eng.hs.iv_hist[iv] + 24);
    }
    eng.hs.recent[eng.hs.rpos] = note;
    eng.hs.rpos = (eng.hs.rpos + 1) & 15;
}

/* meso time: called once per bar */
void harm_bar(void) BANKED
{
    Rng *r = &eng.rng;
    uint8_t tension = ECO8(ECO_TENSION);
    uint8_t stab = ECO8(ECO_STAB);
    uint16_t p;
    uint8_t old = eng.hs.degree, oldroot = eng.hs.root;
    uint8_t simp;

    eng.hs.bars++;

    /* erosion driven simplification */
    simp = (uint8_t)(eng.erosion >> 6);
    if (simp != eng.hs.simplify) { eng.hs.simplify = simp; build_mask(); }

    /* chord change probability */
    p = (uint16_t)proj.harm[HP_CHANGE] * 3 + (uint16_t)proj.macro[MAC_HARMONY] * 2 +
        (uint16_t)proj.macro[MAC_EVOLUTION] + (tension >> 4);
    p -= (uint16_t)((stab * proj.eco[EP_INF_S]) >> 7) > p ? p : (uint16_t)((stab * proj.eco[EP_INF_S]) >> 7);
    if (p > 200) p = 200;
    if (proj.harm[HP_CHANGE] && rng_chance(r, (uint8_t)p)) {
        uint8_t x;
        if (eng.eco.resolving || rng_chance(r, (uint8_t)(stab >> 1))) eng.hs.degree = 0;
        else {
            x = rng8(r);
            if (x < 70) eng.hs.degree = 3;       /* IV  */
            else if (x < 140) eng.hs.degree = 4; /* V   */
            else if (x < 190) eng.hs.degree = 5; /* vi  */
            else if (x < 220) eng.hs.degree = 1; /* ii  */
            else if (x < 245) eng.hs.degree = 2; /* iii */
            else eng.hs.degree = 6;              /* vii */
        }
        if (eng.hs.degree != old) {
            eng.hs.changes++;
            if (eng.hs.degree == 0) { ECO_IMPULSE(ECO_TENSION, -40); ECO_IMPULSE(ECO_STAB, 30); }
            else ECO_IMPULSE(ECO_TENSION, (int16_t)(eng.hs.degree == 4 || eng.hs.degree == 6 ? 40 : 15));
        }
    }

    /* tonal centre drift (DRIFT memory mode, or high harmonic exploration) */
    {
        uint8_t drift_p = 0;
        if (proj.harm[HP_MMODE] == MM_DRIFT) drift_p = (uint8_t)(4 + proj.harm[HP_CHANGE]);
        if (proj.macro[MAC_HARMONY] > 11) drift_p += (uint8_t)(proj.macro[MAC_HARMONY] - 11) * 3;
        drift_p = (uint8_t)((drift_p * (uint8_t)(16 - proj.harm[HP_CSTAB])) >> 3);
        if (drift_p && rng_chance(r, drift_p)) {
            eng.hs.root = (uint8_t)((eng.hs.root + (rng8(r) & 1 ? 7 : 5)) % 12);
            ECO_IMPULSE(ECO_TENSION, 50);
            ECO_IMPULSE(ECO_MEMORY, -40);
        } else if (eng.hs.root != eng.hs.home &&
                   rng_chance(r, (uint8_t)(proj.harm[HP_CSTAB] * 6 + (stab >> 3)))) {
            eng.hs.root = eng.hs.home;          /* return to tonal centre */
            ECO_IMPULSE(ECO_STAB, 40);
        }
        if (eng.hs.root != oldroot) {
            eng.hs.centres[3] = eng.hs.centres[2];
            eng.hs.centres[2] = eng.hs.centres[1];
            eng.hs.centres[1] = eng.hs.centres[0];
            eng.hs.centres[0] = eng.hs.root;
        }
    }
    if (eng.hs.degree != old || eng.hs.root != oldroot) { build_chord(); build_mask(); }
}
