/*
 * AETHER - by Synth Centric
 * form.c : time architecture and long-form structure.
 *
 *  MICRO  : the CLOCKED step clock (16th notes, phase accumulator) or
 *           frame-based FREE timing; called every generated frame.
 *  MESO   : bars (16 steps CLOCKED, 128 frames FREE) drive harmonic change.
 *  MACRO  : seconds drive scenes, transition rules, parameter glides,
 *           TIDE / ERODE processes and FOREVER evolution.
 *
 * Seconds are derived from frames with an 8.8 accumulator (59.73 Hz) so
 * elapsed time stays accurate over long performances; counters are 16-bit
 * with an hour counter, so nothing overflows within any realistic session.
 */
#pragma bank 255
#include <string.h>
#include "engine.h"

#define FRAMES_PER_SEC_88 15291u   /* 59.73 * 256 */

static void set_eco_targets(void)
{
    uint8_t i;
    for (i = 0; i < 4; i++) {
        int16_t t = (int16_t)(eng.form.live.s[SC_ENERGY + i] & 15) * 17 +
                    ((int16_t)proj.eco[EP_BIAS_E + i] - 8) * 12;
        if (t < 0) t = 0;
        if (t > 255) t = 255;
        eng.eco.target[i] = (uint8_t)t;
    }
}

void form_eco_targets(void) BANKED
{
    set_eco_targets();
}

static void apply_scene(uint8_t idx, uint8_t immediate)
{
    FormState *f = &eng.form;
    uint8_t old_mask = immediate ? 0 : f->live.s[SC_MASK];
    uint8_t k, changed = 0;
    idx &= 7;
    memcpy(&f->live, &proj.scene[idx], sizeof(Scene));
    f->scene = idx;
    f->elapsed = 0;
    if (f->live.s[SC_ROOT] < 12 && f->live.s[SC_ROOT] != proj.harm[HP_ROOT]) { proj.harm[HP_ROOT] = f->live.s[SC_ROOT]; changed = 1; }
    if (f->live.s[SC_SCALE] < NUM_SCALES && f->live.s[SC_SCALE] != proj.harm[HP_SCALE]) { proj.harm[HP_SCALE] = f->live.s[SC_SCALE]; changed = 1; }
    if (f->live.s[SC_HMODE] < 3) proj.harm[HP_MMODE] = f->live.s[SC_HMODE];
    if (f->live.s[SC_TMODE] < 2) proj.glob[GP_TMODE] = f->live.s[SC_TMODE];
    f->tempo_tgt = f->live.s[SC_TEMPO] >= 30 ? f->live.s[SC_TEMPO] : proj.glob[GP_TEMPO];
    for (k = 0; k < NUM_MACROS; k++) {
        uint8_t v = f->live.s[SC_MAC + k];
        f->mac_tgt[k] = v <= 15 ? v : proj.macro[k];
        if (immediate) proj.macro[k] = f->mac_tgt[k];
    }
    if (immediate) proj.glob[GP_TEMPO] = f->tempo_tgt;
    f->glide = immediate ? 0 : (uint8_t)(f->live.s[SC_GLIDE] * 4);
    set_eco_targets();
    if (changed) harm_refresh();
    agent_on_scene(old_mask, f->live.s[SC_MASK], immediate ? 0 : f->live.s[SC_GLIDE]);
}

void form_reset(void) BANKED
{
    FormState *f = &eng.form;
    uint8_t s = proj.glob[GP_START] & 7;
    f->elapsed = 0;
    f->total_s = 0;
    f->hours = 0;
    f->sec_acc = 0;
    f->clk = 0;
    f->bar_steps = 0;
    f->glide = 0;
    f->ending = 0;
    f->transitions = 0;
    f->forever_gen = 0;
    f->pending = 0xFF;
    if (!proj.scene[s].s[SC_ON]) s = 0;
    apply_scene(s, 1);
}

void form_goto(uint8_t scene, uint8_t immediate) BANKED
{
    apply_scene(scene, immediate);
    eng.form.transitions++;
    eng.form.ending = 0;
}

void form_store_scene(uint8_t i) BANKED
{
    Scene *sc = &proj.scene[i & 7];
    uint8_t k;
    sc->s[SC_MASK] = eng.form.live.s[SC_MASK];
    sc->s[SC_ROOT] = proj.harm[HP_ROOT];
    sc->s[SC_SCALE] = proj.harm[HP_SCALE];
    for (k = 0; k < NUM_MACROS; k++) sc->s[SC_MAC + k] = proj.macro[k];
    sc->s[SC_TMODE] = proj.glob[GP_TMODE];
    sc->s[SC_TEMPO] = proj.glob[GP_TEMPO];
    for (k = 0; k < 4; k++) sc->s[SC_ENERGY + k] = (uint8_t)(ECO8(k) / 17);
    sc->s[SC_HMODE] = proj.harm[HP_MMODE];
    sc->s[SC_ON] = 1;
    if (!sc->s[SC_LEN]) sc->s[SC_LEN] = 8;
}

static uint8_t scene_ok(uint8_t i) { return proj.scene[i & 7].s[SC_ON]; }

static uint8_t next_enabled(uint8_t from)
{
    uint8_t k, i;
    for (k = 1; k <= NUM_SCENES; k++) {
        i = (uint8_t)((from + k) & 7);
        if (scene_ok(i)) return i;
    }
    return from;
}

static uint8_t random_scene(void)
{
    /* ecology-weighted: favour scenes whose energy target is close to the
     * present energy, so the form follows the state the music is in */
    uint8_t w[NUM_SCENES], k, e = (uint8_t)(ECO8(ECO_ENERGY) / 17);
    uint16_t total = 0, pick;
    for (k = 0; k < NUM_SCENES; k++) {
        uint8_t t, d;
        w[k] = 0;
        if (!scene_ok(k) || k == eng.form.scene) continue;
        t = proj.scene[k].s[SC_ENERGY] & 15;
        d = t > e ? (uint8_t)(t - e) : (uint8_t)(e - t);
        w[k] = (uint8_t)(18 - d);
        total += w[k];
    }
    if (!total) return eng.form.scene;
    pick = (uint16_t)(((uint32_t)rng16(&eng.rng) * total) >> 16);
    for (k = 0; k < NUM_SCENES; k++) {
        if (pick < w[k]) return k;
        pick -= w[k];
    }
    return eng.form.scene;
}

static void forever_mutate(void)
{
    /* FOREVER: evolve the runtime copy of the scene so successive visits differ */
    Scene *l = &eng.form.live;
    Rng *r = &eng.rng;
    uint8_t evo = proj.macro[MAC_EVOLUTION];
    uint8_t k;
    eng.form.forever_gen++;
    if (rng_chance(r, (uint8_t)(40 + evo * 8))) {
        uint8_t bit = (uint8_t)(1 << rng_below(r, NUM_AGENTS));
        uint8_t m = (uint8_t)(l->s[SC_MASK] ^ bit);
        uint8_t c = 0;
        for (k = 0; k < 8; k++) if (m & (1 << k)) c++;
        if (c >= 2) { agent_on_scene(l->s[SC_MASK], m, 2); l->s[SC_MASK] = m; }
    }
    if (rng_chance(r, (uint8_t)(20 + evo * 6))) {
        proj.harm[HP_ROOT] = (uint8_t)((proj.harm[HP_ROOT] + (rng8(r) & 1 ? 7 : 5)) % 12);
        harm_refresh();
    }
    for (k = 0; k < NUM_MACROS; k++) {
        int8_t v = (int8_t)eng.form.mac_tgt[k] + rng_spread(r, 2);
        if (v < 0) v = 0;
        if (v > 15) v = 15;
        eng.form.mac_tgt[k] = (uint8_t)v;
    }
    for (k = 0; k < 4; k++) {
        int8_t v = (int8_t)(l->s[SC_ENERGY + k] & 15) + rng_spread(r, 2);
        if (v < 0) v = 0;
        if (v > 15) v = 15;
        l->s[SC_ENERGY + k] = (uint8_t)v;
    }
    set_eco_targets();
    if (eng.form.glide == 0) eng.form.glide = 20;
}

static void advance(void)
{
    uint8_t nx = eng.form.live.s[SC_NEXT];
    uint8_t forever = proj.glob[GP_FOREVER];
    uint8_t target;
    if (nx < NUM_SCENES && scene_ok(nx)) target = nx;
    else if (nx == NX_RANDOM) target = random_scene();
    else if (nx == NX_END) {
        if (!forever) { eng.form.ending = 1; return; }
        target = rng_chance(&eng.rng, 128) ? random_scene() : next_enabled(eng.form.scene);
    } else target = next_enabled(eng.form.scene);
    form_goto(target, 0);
    if (forever) forever_mutate();
}

static uint8_t eco_condition(uint8_t c)
{
    uint8_t v = ECO8((c >> 1) & 3);
    return (c & 1) ? v < 64 : v > 192;
}

static void form_second(void)
{
    FormState *f = &eng.form;
    uint16_t len;
    uint8_t k;

    f->elapsed++;
    if (++f->total_s >= 3600) { f->total_s = 0; f->hours++; }
    agents_second();

    /* controlled parameter glides */
    if (f->glide) {
        for (k = 0; k < NUM_MACROS; k++) {
            int8_t d = (int8_t)f->mac_tgt[k] - (int8_t)proj.macro[k];
            if (d) {
                int8_t st = (int8_t)(d / (int8_t)f->glide);
                if (!st) st = d > 0 ? 1 : -1;
                proj.macro[k] = (uint8_t)((int8_t)proj.macro[k] + st);
            }
        }
        {
            int16_t d = (int16_t)f->tempo_tgt - (int16_t)proj.glob[GP_TEMPO];
            if (d) {
                int16_t st = d / f->glide;
                if (!st) st = d > 0 ? 1 : -1;
                proj.glob[GP_TEMPO] = (uint8_t)((int16_t)proj.glob[GP_TEMPO] + st);
            }
        }
        f->glide--;
        if (!f->glide) {
            for (k = 0; k < NUM_MACROS; k++) proj.macro[k] = f->mac_tgt[k];
            proj.glob[GP_TEMPO] = f->tempo_tgt;
        }
    }

    if (f->ending) {
        if (++f->ending > 14) { f->ending = 0; engine_set_playing(0); }
        return;
    }

    len = (uint16_t)(f->live.s[SC_LEN] ? f->live.s[SC_LEN] : 8) * 15;
    switch (f->live.s[SC_TRANS]) {
    case TR_TIME:
        if (f->elapsed >= len) advance();
        break;
    case TR_PROB:
        if (f->elapsed >= 30 && (f->elapsed % 15) == 0 &&
            rng_chance(&eng.rng, (uint8_t)(f->live.s[SC_TPARAM] * 16 + 8))) advance();
        else if (f->elapsed >= len * 3) advance();
        break;
    case TR_ECO:
        if ((f->elapsed >= 30 && eco_condition(f->live.s[SC_TPARAM])) || f->elapsed >= len * 2) advance();
        break;
    default:
        if (proj.glob[GP_FOREVER] && f->elapsed >= len * 2) advance();
        break;
    }
}

void form_tick(void) BANKED
{
    FormState *f = &eng.form;
    eng.step = 0;
    if (proj.glob[GP_TMODE] == TM_CLOCKED) {
        uint16_t old = f->clk;
        f->clk += (uint16_t)proj.glob[GP_TEMPO] * 73;
        if (f->clk < old) {
            eng.step = 1;
            if (++f->bar_steps >= 16) { f->bar_steps = 0; harm_bar(); }
        }
    } else if ((eng.ticks & 127) == 0) {
        harm_bar();
    }
    f->sec_acc += 256;
    if (f->sec_acc >= FRAMES_PER_SEC_88) {
        f->sec_acc -= FRAMES_PER_SEC_88;
        form_second();
    }
}
