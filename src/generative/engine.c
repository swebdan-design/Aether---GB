/*
 * AETHER - by Synth Centric
 * engine.c : generator heartbeat, note emission, intelligent voice
 * allocation, event history, phrase players and the noise texture process.
 *
 * Timing architecture
 * -------------------
 *  The VBlank interrupt (59.73 Hz) is the audio clock: it advances
 *  audio_now and plays queued events whose time stamp has arrived.
 *  The generator runs in the main loop and produces events for future
 *  frames (gtime), staying up to LOOKAHEAD frames ahead of playback.
 *  Generation is a pure function of the project, the seed and the user
 *  input history: it never depends on how long UI drawing takes, so a
 *  slow screen redraw only consumes look-ahead, it never skips or shifts
 *  musical decisions.
 *
 *  Micro time : one engine_tick() per frame (notes, modulation, texture)
 *  Meso time  : bars (harmony), phrase players, ecology updates
 *  Macro time : seconds -> scenes, transitions, FOREVER evolution
 */
#pragma bank 255
#include <string.h>
#include "engine.h"
#include "audio.h"

Engine eng;

static const uint16_t dur_tab[16] = { 4, 6, 8, 12, 16, 24, 32, 48, 64, 96, 128, 192, 256, 384, 512, 768 };
static const uint16_t rate_tab[16] = { 6, 8, 12, 16, 24, 32, 48, 64, 96, 128, 180, 240, 360, 480, 720, 960 };
static const uint16_t step_tab[16] = { 1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64, 96, 128, 192, 255 };

#define MIN_HOLD 10        /* frames a note is guaranteed before it may be stolen */

uint16_t dur_frames(uint8_t idx) BANKED { return dur_tab[idx & 15]; }
uint16_t rate_frames(uint8_t idx) BANKED { return rate_tab[idx & 15]; }
uint16_t rate_steps(uint8_t idx) BANKED { return step_tab[idx & 15]; }

/* ------------------------------------------------------ voice allocation */
static uint8_t role_accepts(uint8_t ch, uint16_t dur, uint8_t prio)
{
    switch (proj.glob[GP_ROLE0 + ch]) {
    case ROLE_DRONE:    return dur >= 120 || prio >= 6;
    case ROLE_MELODY:   return dur < 240;
    case ROLE_PARTICLE: return dur <= 48;
    case ROLE_OFF:      return 0;
    default:            return 1;
    }
}

uint8_t voice_alloc(uint8_t itype, uint8_t prio, uint16_t dur, uint8_t prefer1) BANKED
{
    uint8_t cand[2], nc, i, best = 0xFF;
    uint8_t best_prio = 255;
    uint16_t best_age = 0;

    if (itype == IT_PULSE) {
        nc = 2;
        cand[0] = prefer1 ? CH_PULSE1 : CH_PULSE2;
        cand[1] = prefer1 ? CH_PULSE2 : CH_PULSE1;
    } else {
        nc = 1;
        cand[0] = itype == IT_WAVE ? CH_WAVE : CH_NOISE;
    }
    /* 1. a free compatible channel */
    for (i = 0; i < nc; i++) {
        uint8_t ch = cand[i];
        if (!role_accepts(ch, dur, prio)) continue;
        if ((int16_t)(eng.vc[ch].until - eng.gtime) <= 0) return ch;
    }
    /* 2. steal: only lower priority, unprotected, held long enough */
    for (i = 0; i < nc; i++) {
        uint8_t ch = cand[i];
        VChan *v = &eng.vc[ch];
        uint16_t age = (uint16_t)(eng.gtime - v->start);
        if (!role_accepts(ch, dur, prio)) continue;
        if (age < MIN_HOLD) continue;                   /* anti-retrigger */
        if (v->protect && prio < 7) continue;            /* drones protected */
        if (prio > v->prio || (prio == v->prio && age > 48)) {
            if (v->prio < best_prio || (v->prio == best_prio && age > best_age)) {
                best = ch; best_prio = v->prio; best_age = age;
            }
        }
    }
    if (best != 0xFF) eng.st_stolen++;
    return best;
}

/* ---------------------------------------------------------------- pan */
static uint8_t pan_for(uint8_t agent)
{
    uint8_t mode = agent < NUM_AGENTS ? proj.agent[agent].a[AP_PAN] : 0;
    int16_t spread = (int16_t)proj.glob[GP_SPREAD] * 6 + (int16_t)proj.macro[MAC_SPACE] * 6 + eng.mod[MD_PAN];
    if (spread < 0) spread = 0;
    if (spread > 255) spread = 255;
    switch (mode) {
    case 1: return 1;
    case 2: return 2;
    case 3:
        eng.ag[agent].alt ^= 1;
        return (uint8_t)(eng.ag[agent].alt ? 1 : 2);
    case 4:
        if (rng_chance(&eng.rng, (uint8_t)spread)) return (uint8_t)(1 + (rng8(&eng.rng) & 1));
        return 3;
    default:
        if (spread > 96 && rng_chance(&eng.rng, (uint8_t)(spread - 96))) return (uint8_t)(1 + (rng8(&eng.rng) & 1));
        return 3;
    }
}

/* --------------------------------------------------------------- history */
uint8_t hist_recent(uint8_t back) BANKED
{
    return (uint8_t)((eng.hpos + HIST_N - 1 - back) & (HIST_N - 1));
}

static void hist_push(uint8_t note, uint16_t dur, uint8_t agent, uint8_t ch)
{
    HistEv *h = &eng.hist[eng.hpos];
    h->time = eng.gtime;
    h->note = note;
    h->dur4 = dur > 1020 ? 255 : (uint8_t)(dur >> 2);
    h->agent = agent;
    h->ch = ch;
    eng.hpos = (eng.hpos + 1) & (HIST_N - 1);
    if (eng.hcount < HIST_N) eng.hcount++;
}

/* ------------------------------------------------------------------ emit */
uint8_t emit_note(uint8_t agent, uint8_t inst, uint8_t note, uint8_t dur4,
                  uint8_t vol, uint8_t prio, uint8_t flags) BANKED
{
    const uint8_t *I;
    uint8_t itype, ch, n, sens = 8, pitched;
    uint16_t dur;
    NoteEvent e;

    inst &= 7;
    I = proj.inst[inst].p;
    itype = I[IP_TYPE];
    pitched = itype != IT_NOISE;
    if (agent < NUM_AGENTS) sens = proj.agent[agent].a[AP_ECO];

    /* ERODE: probabilistic deletion of material */
    if (!(flags & EMF_NOERODE) && eng.erosion > 16 && agent != AG_ERODE) {
        if (rng_chance(&eng.rng, (uint8_t)(eng.erosion >> 2))) {
            eng.st_eroded++;
            ECO_IMPULSE(ECO_ENERGY, -3);
            return 0xFF;
        }
    }

    /* pitch */
    if (pitched) {
        int16_t x = (int16_t)note + ((int16_t)I[IP_OCT] - 2) * 12 + (eng.mod[MD_PITCH] / 32);
        int16_t lo = itype == IT_WAVE ? 24 : 36;
        int16_t hi = itype == IT_WAVE ? 95 : 107;
        while (x < lo) x += 12;
        while (x > hi) x -= 12;
        n = (uint8_t)x;
    } else {
        int16_t x;
        if (note < 24 && !(flags & EMF_RECALL) && agent >= NUM_AGENTS) x = note;
        else x = (int16_t)I[IP_NBASE] + rng_spread(&eng.rng, I[IP_NSPREAD] & 7);
        x += eng.mod[MD_NOISE] / 16;
        if (x < 0) x = 0;
        if (x > 23) x = 23;
        n = (uint8_t)x;
    }

    /* duration */
    dur = (uint16_t)dur4 * 4;
    dur = (uint16_t)((dur * (I[IP_LEN] ? I[IP_LEN] : 8)) >> 3);
    if (eng.mod[MD_LENGTH]) dur = scale128(dur, (uint8_t)(128 + eng.mod[MD_LENGTH]));
    if (dur < 2) dur = 2;
    if (dur > 4000) dur = 4000;

    /* volume */
    {
        int8_t v = (int8_t)(vol ? vol : I[IP_VOL]);
        if (itype == IT_WAVE && !vol) v = (int8_t)(I[IP_VOL] >= 3 ? 15 : I[IP_VOL] == 2 ? 9 : 4);
        v += eng.mod[MD_VOLUME] / 16;
        if (v < 1) v = 1;
        if (v > 15) v = 15;
        e.vol = (uint8_t)v;
    }

    ch = voice_alloc(itype, prio, dur, (uint8_t)(I[IP_SWPACE] != 0));
    if (ch == 0xFF) { eng.st_dropped++; if (agent < 10) eng.st_ag_drop[agent]++; return 0xFF; }
    if (aq_space() == 0) { eng.st_qfull++; return 0xFF; }

    e.time = eng.gtime;
    e.dur = dur;
    e.ch = ch;
    e.note = n;
    e.inst = inst;
    e.pan = pan_for(agent);
    e.flags = 0;
    if (!I[IP_RETRIG] && eng.vc[ch].agent == agent &&
        (int16_t)(eng.vc[ch].until - eng.gtime) > 0) e.flags |= NEF_LEGATO;
    e.detune = I[IP_RAND] ? rng_spread(&eng.rng, I[IP_RAND]) : 0;
    e.aux = 0;
    if (itype == IT_WAVE) {
        int8_t a = (int8_t)I[IP_WAMT] + eng.mod[MD_WAVE] / 16;
        if (a < 0) a = 0;
        if (a > 8) a = 8;
        if (a && (I[IP_WMORPH] & 3) != (I[IP_WSLOT] & 3)) {
            eng.stage_k ^= 1;
            wave_blend(I[IP_WSLOT], I[IP_WMORPH], (uint8_t)a, wave_stage[eng.stage_k]);
            e.aux = (uint8_t)(AUX_STAGED | eng.stage_k);
        }
    } else if (itype == IT_PULSE && eng.mod[MD_DUTY]) {
        e.aux = (uint8_t)((((int8_t)I[IP_SHAPE] + eng.mod[MD_DUTY] / 32) & 3) + 1);
    }
    aq_push(&e);

    /* virtual channel model (mirrors exactly what the driver will do) */
    eng.vc[ch].until = (uint16_t)(eng.gtime + dur);
    eng.vc[ch].start = eng.gtime;
    eng.vc[ch].prio = prio;
    eng.vc[ch].agent = agent;
    eng.vc[ch].note = n;
    eng.vc[ch].protect = (uint8_t)((flags & EMF_PROTECT) || dur >= 200 ||
                                   proj.glob[GP_ROLE0 + ch] == ROLE_DRONE);
    eng.ch_agent[ch] = agent;

    /* ecology impulses */
    ECO_IMPULSE(ECO_ENERGY, (int16_t)(2 + (sens >> 2) + (agent == AG_SCATTER ? 3 : 0)));
    if (pitched) {
        uint8_t cons = harm_consonance(n);
        uint8_t prev = agent < NUM_AGENTS ? eng.ag[agent].last : 0;
        if (cons < 6) ECO_IMPULSE(ECO_TENSION, (int16_t)(4 + (sens >> 2)));
        else if (cons >= 13) { ECO_IMPULSE(ECO_TENSION, -2); ECO_IMPULSE(ECO_STAB, 1); }
        if (flags & EMF_RECALL) ECO_IMPULSE(ECO_MEMORY, 6);
        else if (eng.hs.pc_hist[n % 12] < 24) ECO_IMPULSE(ECO_MEMORY, -2);
        harm_note(n, prev);
        if (agent < NUM_AGENTS) {
            if (prev) {
                int8_t iv = (int8_t)(n - prev);
                markov_learn(eng.ag[agent].last_iv, iv);
                eng.ag[agent].last_iv = iv;
            }
            eng.ag[agent].last = n;
        }
        hist_push(n, dur, agent, ch);
    }
    eng.eco.notes++;

    /* runtime wavetable evolution */
    if (itype == IT_WAVE && I[IP_MOD]) {
        if (rng_chance(&eng.rng, (uint8_t)(I[IP_MOD] * 8 + proj.macro[MAC_TEXTURE] * 4)))
            wave_evolve(I[IP_WSLOT], (uint8_t)(1 + (proj.macro[MAC_TEXTURE] >> 2)), &eng.rng);
    }

    if (agent < NUM_AGENTS) eng.ag[agent].fired = 12;
    eng.viz[eng.viz_w].note = n;
    eng.viz[eng.viz_w].ch = ch;
    eng.viz[eng.viz_w].agent = agent;
    eng.viz_w = (eng.viz_w + 1) & (VIZ_N - 1);
    eng.st_events++;
    if (agent < 10) eng.st_ag_ev[agent]++;
    return ch;
}

/* --------------------------------------------------------- phrase players */
void phrase_start(const MotifNote *n, uint8_t len, uint8_t base, uint8_t agent,
                  uint8_t inst, uint8_t vol, uint8_t tscale, uint16_t delay,
                  uint8_t fragment) BANKED
{
    uint8_t i, slot = 0xFF;
    Phrase *p;
    if (!len) return;
    if (len > MOTIF_MAX) len = MOTIF_MAX;
    for (i = 0; i < NUM_PHRASE; i++) if (!eng.ph[i].active) { slot = i; break; }
    if (slot == 0xFF) {
        /* replace the player that started first among same/lower agents */
        slot = (uint8_t)(eng.ticks % NUM_PHRASE);
    }
    p = &eng.ph[slot];
    memcpy(p->n, n, sizeof(MotifNote) * len);
    p->len = len;
    p->pos = 0;
    p->base = base;
    p->agent = agent;
    p->inst = inst;
    p->vol = vol;
    p->prio = agent < NUM_AGENTS ? proj.agent[agent].a[AP_PRI] : 3;
    p->tscale = tscale ? tscale : 2;
    p->fragment = fragment;
    p->next = (uint16_t)(eng.gtime + delay + (((uint16_t)n[0].dt * 4 * p->tscale) >> 1));
    p->active = 1;
}

static void phrase_tick(void)
{
    uint8_t i;
    Phrase *p = eng.ph;
    for (i = 0; i < NUM_PHRASE; i++, p++) {
        MotifNote *m;
        if (!p->active) continue;
        if ((int16_t)(eng.gtime - p->next) < 0) continue;
        m = &p->n[p->pos];
        if (!(p->fragment && rng_chance(&eng.rng, p->fragment))) {
            int16_t x = (int16_t)p->base + m->rel;
            uint8_t d4 = (uint8_t)(((uint16_t)m->dur * p->tscale) >> 1);
            if (x < 24) x += 12;
            if (x > 107) x -= 12;
            if (d4 == 0) d4 = 1;
            emit_note(p->agent, p->inst, harm_quantize((uint8_t)x), d4, p->vol, p->prio, EMF_RECALL);
        }
        p->pos++;
        if (p->pos >= p->len) { p->active = 0; continue; }
        {
            uint16_t dt = ((uint16_t)p->n[p->pos].dt * 4 * p->tscale) >> 1;
            p->next = (uint16_t)(p->next + (dt ? dt : 1));
        }
    }
}

/* ----------------------------------------------------- noise texture */
static void texture_tick(void)
{
    uint8_t ti = proj.glob[GP_TEXINST];
    const uint8_t *I;
    uint16_t prob;
    uint8_t bit;
    if (ti >= NUM_INST) return;
    /* cheap early-out before touching the instrument (every frame) */
    if (proj.glob[GP_TMODE] == TM_CLOCKED) {
        if (!eng.step) return;
        I = proj.inst[ti].p;
        if (I[IP_TYPE] != IT_NOISE) return;
    } else {
        if (eng.tex_timer) { eng.tex_timer--; return; }
        I = proj.inst[ti].p;
        if (I[IP_TYPE] != IT_NOISE) return;
        eng.tex_timer = (uint8_t)(5 + (15 - (I[IP_DENS] & 15)));
    }
    eng.tex_pos = (eng.tex_pos + 1) & 7;
    bit = (uint8_t)((I[IP_PATTERN] >> eng.tex_pos) & 1);

    /* slowly changing activity */
    if (I[IP_MOD] && (eng.tex_pos == 0)) {
        int16_t a = (int16_t)eng.tex_act + rng_spread(&eng.rng, (uint8_t)(I[IP_MOD] * 3));
        if (a < 32) a = 64 - a;
        if (a > 255) a = 255;
        eng.tex_act = (uint8_t)a;
    }

    prob = (uint16_t)(I[IP_DENS] & 15) * 6;
    prob = (prob * (uint16_t)(proj.macro[MAC_TEXTURE] + 1)) >> 3;
    prob = (prob * (uint16_t)(eng.tex_act + 64)) >> 8;
    prob = (uint16_t)((int16_t)prob + (ECO_INFL(ECO_ENERGY, 10) >> 1));
    if ((int16_t)prob < 0) prob = 0;
    if (!bit) prob = (prob * (I[IP_IRREG] & 15)) >> 4;
    else if (I[IP_IRREG] && rng_chance(&eng.rng, (uint8_t)(I[IP_IRREG] * 5))) prob = 0;
    if (prob > 250) prob = 250;
    if (prob && rng_chance(&eng.rng, (uint8_t)prob)) {
        uint8_t len = I[IP_LEN] & 15;
        uint8_t d4 = (uint8_t)(1 + ((len * len) >> 2));
        emit_note(8, ti, 0xFF, d4, 0, 1, 0);
    }
}

/* ---------------------------------------------------------------- tick */
void engine_tick(void) BANKED
{
    form_tick();
    if ((eng.ticks & 3) == 0) mod_update();
    if ((eng.ticks & 15) == 8) eco_update();
    texture_tick();
    phrase_tick();
    agents_tick();
    eng.gtime++;
    eng.ticks++;
}

void engine_service(uint16_t now) BANKED
{
    uint8_t n = 0;
    int16_t behind;
    if (!eng.playing) { eng.gtime = (uint16_t)(now + 1); return; }
    behind = (int16_t)(now - eng.gtime);
    if (behind > 0 && (uint8_t)(behind > 255 ? 255 : behind) > eng.st_maxbehind)
        eng.st_maxbehind = (uint8_t)(behind > 255 ? 255 : behind);
    while ((int16_t)(eng.gtime - now) < LOOKAHEAD && n < MAX_CATCHUP) {
        if (aq_space() < 8) { eng.st_qfull++; break; }
        engine_tick();
        n++;
    }
}

void engine_stop_all(void) BANKED
{
    uint8_t i;
    audio_all_off();
    for (i = 0; i < NUM_CH; i++) { eng.vc[i].until = eng.gtime; eng.vc[i].protect = 0; eng.vc[i].prio = 0; }
    for (i = 0; i < NUM_PHRASE; i++) eng.ph[i].active = 0;
}

void engine_set_playing(uint8_t on) BANKED
{
    if (on && !eng.playing) {
        eng.gtime = (uint16_t)(audio_time() + 2);
        engine_stop_all();
        eng.playing = 1;
    } else if (!on && eng.playing) {
        eng.playing = 0;
        engine_stop_all();
    }
}

uint8_t engine_audition(uint8_t inst, uint8_t note, uint8_t d4) BANKED
{
    uint8_t ch;
    if (!eng.playing) eng.gtime = (uint16_t)(audio_time() + 1);
    ch = emit_note(9, inst, note, d4, 0, 7, EMF_NOERODE);
    return ch;
}

void engine_restart(void) BANKED
{
    uint16_t seed = (uint16_t)(proj.seed_lo | ((uint16_t)proj.seed_hi << 8));
    uint8_t i;
    rng_seed(&eng.rng, (uint16_t)(seed ^ 0x9E37));
    eng.ticks = 0;
    eng.step = 0;
    for (i = 0; i < NUM_CH; i++) {
        eng.vc[i].until = eng.gtime; eng.vc[i].start = eng.gtime;
        eng.vc[i].prio = 0; eng.vc[i].protect = 0; eng.vc[i].agent = 0xFF; eng.vc[i].note = 0;
        eng.ch_agent[i] = 0xFF;
    }
    memset(eng.hist, 0, sizeof(eng.hist));
    eng.hpos = 0; eng.hcount = 0;
    for (i = 0; i < NUM_PHRASE; i++) eng.ph[i].active = 0;
    eng.tide = 128; eng.tide_phase = 0; eng.tide_tgt = 128;
    eng.erosion = 0;
    eng.tex_timer = 0; eng.tex_pos = 0; eng.tex_act = 160;
    memset(eng.amotif, 0, sizeof(eng.amotif));
    eng.apos = 0;
    eng.viz_w = eng.viz_r = 0;
    eng.st_events = eng.st_dropped = eng.st_stolen = eng.st_eroded = eng.st_qfull = 0;
    eng.st_maxbehind = 0;
    memset(eng.st_ag_ev, 0, sizeof(eng.st_ag_ev));
    memset(eng.st_ag_drop, 0, sizeof(eng.st_ag_drop));
    harm_reset();
    mod_reset();
    form_reset();          /* applies the start scene: macros, key, eco targets */
    harm_refresh();
    eco_reset();
    agents_reset();
    wave_sync_live();
}

void engine_init(void) BANKED
{
    eng.playing = 0;
    eng.gtime = audio_time();
    engine_restart();
}
