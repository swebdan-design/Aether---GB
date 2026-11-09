/*
 * AETHER - by Synth Centric
 * test_main.c : automated host test-suite for the platform independent
 * generative core (the exact C sources that are compiled into the ROM).
 *
 * The "player" below consumes the event queue the same way the VBlank
 * driver does, so voice allocation and timing can be checked precisely.
 * Long-duration tests are deterministic SIMULATIONS of the engine at the
 * Game Boy frame rate; they are not emulator runs.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "engine.h"
#include "audio.h"
#include "storage.h"

extern uint8_t host_sram[32768];

static int tests_run, tests_failed, checks;
#define CHECK(c) do { checks++; if (!(c)) { printf("    FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); fail = 1; } } while (0)
#define TEST(name) static int name(void)
#define RUN(name) do { int fail; tests_run++; printf("  %-38s", #name); fflush(stdout); fail = name(); \
    if (fail) { tests_failed++; printf("FAILED\n"); } else printf("ok\n"); } while (0)

/* ------------------------------------------------------------ player */
typedef struct { uint16_t t; uint16_t dur; uint8_t ch, note, inst, vol, pan, flags; } Ev;
#define MAXEV 200000
static Ev evlog[MAXEV];
static long nev;
static uint16_t ch_until[4], ch_start[4];
static int overlap_violation, type_violation;

static void player_reset(void)
{
    nev = 0; overlap_violation = 0; type_violation = 0;
    memset(ch_until, 0, sizeof(ch_until));
    memset(ch_start, 0, sizeof(ch_start));
}

static void play_due(void)
{
    while (aq_tail != aq_head) {
        NoteEvent *e = &aq[aq_tail];
        uint8_t t;
        if ((int16_t)(e->time - audio_now) > 0) break;
        t = proj.inst[e->inst & 7].p[IP_TYPE];
        if ((t == IT_PULSE && e->ch > 1) || (t == IT_WAVE && e->ch != 2) || (t == IT_NOISE && e->ch != 3)) type_violation++;
        /* a new note on a still-sounding channel must respect the hold time */
        if ((int16_t)(ch_until[e->ch] - e->time) > 0 && (uint16_t)(e->time - ch_start[e->ch]) < 10) overlap_violation++;
        ch_until[e->ch] = (uint16_t)(e->time + e->dur);
        ch_start[e->ch] = e->time;
        if (nev < MAXEV) {
            Ev *v = &evlog[nev++];
            v->t = e->time; v->dur = e->dur; v->ch = e->ch; v->note = e->note; v->inst = e->inst;
            v->vol = e->vol; v->pan = e->pan; v->flags = e->flags;
        }
        aq_tail = (aq_tail + 1) & AQ_MASK;
    }
}

static void start(uint8_t preset)
{
    preset_load(preset);
    audio_init();
    engine_init();
    engine_set_playing(1);
    player_reset();
}

static void run_frames(long n)
{
    long f;
    for (f = 0; f < n && eng.playing; f++) {
        audio_now++;
        engine_service(audio_now);
        play_due();
    }
}
#define MIN(m) ((long)(m) * 3584L)   /* frames per minute at 59.73 Hz */

static uint32_t log_hash(void)
{
    uint32_t h = 2166136261u;
    long i;
    for (i = 0; i < nev; i++) {
        const uint8_t *b = (const uint8_t *)&evlog[i];
        size_t k;
        for (k = 0; k < sizeof(Ev); k++) h = (h ^ b[k]) * 16777619u;
    }
    return h;
}

/* =============================================================== tests */
TEST(t_rng_reproducible)
{
    int fail = 0, i, same = 1, diff = 0;
    Rng a, b, c;
    rng_seed(&a, 1234); rng_seed(&b, 1234); rng_seed(&c, 1235);
    for (i = 0; i < 1000; i++) {
        uint16_t x = rng16(&a), y = rng16(&b), z = rng16(&c);
        if (x != y) same = 0;
        if (x != z) diff++;
    }
    CHECK(same);
    CHECK(diff > 990);
    rng_seed(&a, 0);
    CHECK(a.s != 0);                                   /* zero seed is remapped */
    {   /* full period 65535 */
        uint16_t first;
        long n = 0;
        rng_seed(&a, 1); first = a.s;
        do { rng16(&a); n++; } while (a.s != first && n < 70000);
        CHECK(n == 65535);
    }
    {   /* rng_below stays in range and covers it */
        int hit[7] = {0};
        rng_seed(&a, 99);
        for (i = 0; i < 7000; i++) { uint8_t v = rng_below(&a, 7); CHECK(v < 7); if (v < 7) hit[v]++; }
        for (i = 0; i < 7; i++) CHECK(hit[i] > 700);
    }
    return fail;
}

TEST(t_scale_quantisation)
{
    int fail = 0;
    uint8_t sc, root, n;
    preset_load(0);
    for (sc = 0; sc < NUM_SCALES; sc++) {
        for (root = 0; root < 12; root++) {
            uint16_t rel = scale_mask(sc);
            proj.harm[HP_SCALE] = sc; proj.harm[HP_ROOT] = root;
            harm_reset();
            for (n = 30; n < 100; n++) {
                uint8_t q = harm_quantize(n);
                uint8_t pcrel = (uint8_t)((q + 12 - root) % 12);
                CHECK((rel >> pcrel) & 1);
                CHECK(q + 6 >= n && q <= n + 6);
            }
        }
    }
    /* harmonic movement in scale steps */
    proj.harm[HP_SCALE] = SCALE_MAJOR; proj.harm[HP_ROOT] = 0; harm_reset();
    CHECK(harm_degree(60, 1) == 62);
    CHECK(harm_degree(60, 2) == 64);
    CHECK(harm_degree(60, -1) == 59);
    CHECK(harm_degree(60, 7) == 72);
    /* user pitch-class set */
    proj.harm[HP_SCALE] = SCALE_USER; proj.harm[HP_UMASKL] = 0x81; proj.harm[HP_UMASKH] = 0; /* root + fifth */
    harm_reset();
    for (n = 40; n < 90; n++) { uint8_t q = harm_quantize(n) % 12; CHECK(q == 0 || q == 7); }
    return fail;
}

TEST(t_pitch_choice_in_range_and_scale)
{
    int fail = 0, i, chroma = 0;
    Rng r;
    preset_load(0);
    eng.gtime = 0;
    proj.harm[HP_SCALE] = SCALE_DORIAN; proj.harm[HP_ROOT] = 2;
    harm_reset(); eco_reset();
    rng_seed(&r, 7);
    for (i = 0; i < 5000; i++) {
        uint8_t lo = (uint8_t)(40 + rng_below(&r, 20)), hi = (uint8_t)(lo + 4 + rng_below(&r, 24));
        uint8_t t = (uint8_t)(lo + rng_below(&r, (uint8_t)(hi - lo + 1)));
        uint8_t n = harm_choose(t, lo, hi, (uint8_t)(i & 1 ? t : 0), 15, (uint8_t)(1 + (i % 6)), &r);
        CHECK(n >= lo && n <= hi);
        CHECK((eng.hs.mask >> (n % 12)) & 1);                       /* strict adherence */
        n = harm_choose(t, lo, hi, 0, 0, 4, &r);
        CHECK(n >= lo && n <= hi);
        if (!((eng.hs.mask >> (n % 12)) & 1)) chroma++;
    }
    CHECK(chroma > 0);                                              /* low adherence allows colour */
    /* global register limits always win */
    proj.harm[HP_LO] = 50; proj.harm[HP_HI] = 70;
    for (i = 0; i < 1000; i++) { uint8_t n = harm_choose(30, 20, 100, 0, 10, 12, &r); CHECK(n >= 50 && n <= 70); }
    return fail;
}

TEST(t_consonance_preference)
{
    /* high consonance preference + no dissonance -> mostly chord tones */
    int fail = 0, i, chord_hi = 0, chord_lo = 0;
    Rng r;
    preset_load(0);
    proj.harm[HP_SCALE] = SCALE_MAJOR; proj.harm[HP_ROOT] = 0;
    proj.harm[HP_CONS] = 15; proj.harm[HP_DISS] = 0; proj.macro[MAC_HARMONY] = 0;
    harm_reset(); eco_reset();
    eng.eco.v[ECO_TENSION] = 0;
    rng_seed(&r, 3);
    for (i = 0; i < 3000; i++) { uint8_t n = harm_choose(64, 55, 79, 0, 15, 7, &r); if (eng.hs.chord & (1u << (n % 12))) chord_hi++; }
    proj.harm[HP_CONS] = 0; proj.harm[HP_DISS] = 15; proj.macro[MAC_HARMONY] = 15;
    eng.eco.v[ECO_TENSION] = 250 << 8;
    for (i = 0; i < 3000; i++) { uint8_t n = harm_choose(64, 55, 79, 0, 15, 7, &r); if (eng.hs.chord & (1u << (n % 12))) chord_lo++; }
    CHECK(chord_hi > 2000);
    CHECK(chord_lo < chord_hi / 2);
    return fail;
}

TEST(t_engine_ranges)
{
    int fail = 0;
    long i;
    uint8_t p;
    for (p = 0; p < NUM_PRESETS; p++) {
        start(p);
        run_frames(MIN(15));
        CHECK(nev > 50);
        CHECK(type_violation == 0);
        for (i = 0; i < nev; i++) {
            Ev *e = &evlog[i];
            uint8_t t = proj.inst[e->inst].p[IP_TYPE];
            if (t == IT_PULSE) CHECK(e->note >= 36 && e->note <= 107);
            else if (t == IT_WAVE) CHECK(e->note >= 24 && e->note <= 95);
            else CHECK(e->note <= 23);
            CHECK(e->vol >= 1 && e->vol <= 15);
            CHECK(e->dur >= 2);
            CHECK(e->pan >= 1 && e->pan <= 3);
        }
    }
    return fail;
}

TEST(t_agent_rules)
{
    int fail = 0, a;
    long i;
    /* run each agent alone and check its register and behaviour */
    for (a = 0; a < NUM_AGENTS; a++) {
        uint8_t k;
        if (a == AG_ERODE) continue;
        preset_load(0);
        for (k = 0; k < NUM_AGENTS; k++) proj.agent[k].a[AP_EN] = (k == a) || (a == AG_ECHO && k == AG_ORBIT);
        for (k = 0; k < NUM_SCENES; k++) proj.scene[k].s[SC_MASK] = 0xFF;
        proj.glob[GP_TEXINST] = 8;
        proj.mod[0].src = MS_OFF; proj.mod[1].src = MS_OFF;
        proj.agent[a].a[AP_ACT] = 15;
        proj.agent[a].a[AP_LO] = 60; proj.agent[a].a[AP_HI] = 72;
        proj.agent[a].a[AP_HARM] = 15;
        proj.agent[a].a[AP_INST] = 0; proj.agent[a].a[AP_INST2] = 8;
        proj.inst[0].p[IP_OCT] = 2;
        if (a == AG_TIDE) { proj.agent[a].a[AP_P3] = 15; }
        audio_init(); engine_init(); engine_set_playing(1); player_reset();
        run_frames(MIN(4));
        CHECK(eng.st_ag_ev[a] > 3);
        for (i = 0; i < NUM_AGENTS; i++) if (i != a && !(a == AG_ECHO && i == AG_ORBIT)) CHECK(eng.st_ag_ev[i] == 0);
        if (a != AG_ECHO && a != AG_MEMORY)
            for (i = 0; i < nev; i++) {
                /* register (TIDE shifts registers of others only) + strict scale */
                CHECK(evlog[i].note >= 59 && evlog[i].note <= 73);
            }
    }
    /* DRIFT: consecutive notes move at most MAX STEP (+bias) scale degrees */
    {
        uint8_t k;
        int big = 0;
        preset_load(0);
        for (k = 0; k < NUM_AGENTS; k++) proj.agent[k].a[AP_EN] = k == AG_DRIFT;
        for (k = 0; k < NUM_SCENES; k++) proj.scene[k].s[SC_MASK] = 0xFF;
        proj.glob[GP_TEXINST] = 8; proj.mod[0].src = MS_OFF; proj.mod[1].src = MS_OFF;
        proj.agent[AG_DRIFT].a[AP_P1] = 1; proj.agent[AG_DRIFT].a[AP_P2] = 7; proj.agent[AG_DRIFT].a[AP_ACT] = 15;
        proj.agent[AG_DRIFT].a[AP_LO] = 48; proj.agent[AG_DRIFT].a[AP_HI] = 84; proj.agent[AG_DRIFT].a[AP_HARM] = 15;
        proj.agent[AG_DRIFT].a[AP_INST] = 0; proj.inst[0].p[IP_OCT] = 2;
        proj.macro[MAC_MOTION] = 0; proj.harm[HP_REGSEP] = 0;
        audio_init(); engine_init(); engine_set_playing(1); player_reset();
        run_frames(MIN(5));
        CHECK(nev > 20);
        for (i = 1; i < nev; i++) {
            int d = abs((int)evlog[i].note - (int)evlog[i - 1].note);
            if (d > 7) big++;          /* > 2 scale steps + one harmonic adjustment */
        }
        CHECK(big < nev / 20);
    }
    /* disabled agents and agents outside the scene mask stay silent */
    {
        uint8_t k;
        preset_load(0);
        for (k = 0; k < NUM_SCENES; k++) proj.scene[k].s[SC_MASK] = (uint8_t)(1 << AG_ORBIT);
        proj.agent[AG_ORBIT].a[AP_EN] = 0;
        proj.glob[GP_TEXINST] = 8;
        audio_init(); engine_init(); engine_set_playing(1); player_reset();
        run_frames(MIN(3));
        CHECK(nev == 0);
    }
    return fail;
}

TEST(t_ecology_bounds_and_damping)
{
    int fail = 0, i;
    preset_load(0);
    audio_init(); engine_init();
    /* hammer with maximal impulses: values stay bounded and below hard saturation */
    for (i = 0; i < 2000; i++) {
        uint8_t k;
        for (k = 0; k < 4; k++) eng.eco.acc[k] = (i & 64) ? -30000 : 30000;
        eco_update();
        for (k = 0; k < 4; k++) CHECK(eng.eco.v[k] <= 65535);
    }
    /* after the input stops, every variable returns to its baseline */
    for (i = 0; i < 600; i++) { eng.eco.notes = 1; eco_update(); }
    for (i = 0; i < 4; i++) CHECK(abs((int)(eng.eco.v[i] >> 8) - (int)eng.eco.target[i]) < 40);
    for (i = 0; i < 4; i++) CHECK((eng.eco.v[i] >> 8) > 5 && (eng.eco.v[i] >> 8) < 250);
    /* positive pressure saturates softly: not pinned at the ceiling */
    for (i = 0; i < 300; i++) { eng.eco.acc[ECO_ENERGY] = 60; eco_update(); }
    CHECK((eng.eco.v[ECO_ENERGY] >> 8) < 250);
    CHECK((eng.eco.v[ECO_ENERGY] >> 8) > eng.eco.target[ECO_ENERGY]);
    /* tension resolution engages and releases */
    eng.eco.v[ECO_TENSION] = 230 << 8;
    eco_update();
    CHECK(eng.eco.resolving == 1);
    for (i = 0; i < 400 && eng.eco.resolving; i++) eco_update();
    CHECK(eng.eco.resolving == 0);
    /* freeze holds the state */
    proj.eco[EP_FREEZE] = 1;
    {
        uint16_t v = eng.eco.v[ECO_MEMORY];
        for (i = 0; i < 100; i++) { eng.eco.acc[ECO_MEMORY] = 100; eco_update(); }
        CHECK(eng.eco.v[ECO_MEMORY] == v);
    }
    return fail;
}

TEST(t_ecology_feedback_changes_music)
{
    /* the ecology measurably changes generation: same seed, different bias */
    int fail = 0;
    long low, high;
    uint32_t h1, h2;
    preset_load(0);
    proj.eco[EP_BIAS_E] = 0; proj.eco[EP_INF_E] = 15;
    audio_init(); engine_init(); engine_set_playing(1); player_reset();
    run_frames(MIN(10));
    low = nev; h1 = log_hash();
    preset_load(0);
    proj.eco[EP_BIAS_E] = 15; proj.eco[EP_INF_E] = 15;
    audio_init(); engine_init(); engine_set_playing(1); player_reset();
    run_frames(MIN(10));
    high = nev; h2 = log_hash();
    CHECK(h1 != h2);
    CHECK(high > low * 11 / 10);
    /* agent activity feeds back: scatter-heavy settings raise ENERGY */
    {
        unsigned e_quiet, e_busy;
        preset_load(0);
        proj.agent[AG_SCATTER].a[AP_ACT] = 0;
        audio_init(); engine_init(); engine_set_playing(1); player_reset();
        run_frames(MIN(2));
        e_quiet = eng.eco.v[ECO_ENERGY] >> 8;
        preset_load(0);
        proj.agent[AG_SCATTER].a[AP_ACT] = 15; proj.agent[AG_SCATTER].a[AP_RATE] = 2;
        proj.agent[AG_SCATTER].a[AP_P2] = 0; proj.agent[AG_SCATTER].a[AP_ECO] = 15;
        audio_init(); engine_init(); engine_set_playing(1); player_reset();
        run_frames(MIN(2));
        e_busy = eng.eco.v[ECO_ENERGY] >> 8;
        CHECK(e_busy > e_quiet + 10);
    }
    return fail;
}

TEST(t_voice_allocation)
{
    int fail = 0;
    uint8_t ch;
    preset_load(0);
    audio_init(); engine_init();
    eng.playing = 1;
    eng.gtime = 100;
    /* free channels are used first, pulse prefers the requested channel */
    ch = voice_alloc(IT_PULSE, 3, 50, 1); CHECK(ch == CH_PULSE1);
    eng.vc[0].until = 300; eng.vc[0].start = 100; eng.vc[0].prio = 3; eng.vc[0].protect = 0;
    ch = voice_alloc(IT_PULSE, 3, 50, 1); CHECK(ch == CH_PULSE2);
    eng.vc[1].until = 300; eng.vc[1].start = 100; eng.vc[1].prio = 5; eng.vc[1].protect = 0;
    ch = voice_alloc(IT_WAVE, 1, 50, 0); CHECK(ch == CH_WAVE);
    ch = voice_alloc(IT_NOISE, 1, 50, 0); CHECK(ch == CH_NOISE);
    /* anti-retrigger: no stealing before the hold time */
    eng.gtime = 105;
    ch = voice_alloc(IT_PULSE, 7, 50, 1); CHECK(ch == 0xFF);
    /* lower priority cannot steal; higher steals the lowest-priority voice */
    eng.gtime = 140;
    ch = voice_alloc(IT_PULSE, 2, 50, 1); CHECK(ch == 0xFF);
    ch = voice_alloc(IT_PULSE, 6, 50, 1); CHECK(ch == CH_PULSE1);
    /* protected drone survives anything but top priority */
    eng.vc[2].until = 2000; eng.vc[2].start = 100; eng.vc[2].prio = 2; eng.vc[2].protect = 1;
    ch = voice_alloc(IT_WAVE, 6, 50, 0); CHECK(ch == 0xFF);
    ch = voice_alloc(IT_WAVE, 7, 50, 0); CHECK(ch == CH_WAVE);
    /* channel roles */
    proj.glob[GP_ROLE0 + 3] = ROLE_OFF;
    ch = voice_alloc(IT_NOISE, 7, 50, 0); CHECK(ch == 0xFF);
    proj.glob[GP_ROLE0 + 2] = ROLE_DRONE; eng.vc[2].until = 0;
    ch = voice_alloc(IT_WAVE, 3, 30, 0); CHECK(ch == 0xFF);
    ch = voice_alloc(IT_WAVE, 3, 300, 0); CHECK(ch == CH_WAVE);
    proj.glob[GP_ROLE0 + 0] = ROLE_PARTICLE; proj.glob[GP_ROLE0 + 1] = ROLE_PARTICLE;
    eng.vc[0].until = 0; eng.vc[1].until = 0;
    ch = voice_alloc(IT_PULSE, 3, 200, 1); CHECK(ch == 0xFF);
    ch = voice_alloc(IT_PULSE, 3, 20, 1); CHECK(ch == CH_PULSE1);
    /* in real playback: never more than the hardware channels, hold respected */
    start(5);
    run_frames(MIN(20));
    CHECK(type_violation == 0);
    CHECK(overlap_violation == 0);
    CHECK(eng.st_stolen > 0);
    return fail;
}

TEST(t_drone_protection)
{
    /* a TIDE drone on the wave channel is never interrupted by other agents */
    int fail = 0;
    long i;
    uint8_t k;
    preset_load(0);
    for (k = 0; k < NUM_SCENES; k++) proj.scene[k].s[SC_MASK] = 0xFF;
    proj.agent[AG_TIDE].a[AP_P3] = 15; proj.agent[AG_TIDE].a[AP_ACT] = 15;
    for (k = 0; k < NUM_AGENTS; k++) if (k != AG_TIDE) { proj.agent[k].a[AP_INST] = 5; proj.agent[k].a[AP_ACT] = 15; }
    audio_init(); engine_init(); engine_set_playing(1); player_reset();
    run_frames(MIN(6));
    for (i = 1; i < nev; i++) {
        long j;
        if (evlog[i].ch != CH_WAVE || evlog[i].inst != 4) continue;  /* drone onset */
        for (j = i + 1; j < nev; j++) {
            if (evlog[j].ch != CH_WAVE) continue;
            CHECK((uint16_t)(evlog[j].t - evlog[i].t) >= evlog[i].dur);
            break;
        }
    }
    return fail;
}

TEST(t_event_queue_capacity)
{
    int fail = 0, i;
    NoteEvent e;
    memset(&e, 0, sizeof(e));
    aq_head = aq_tail = 0;
    CHECK(aq_space() == AQ_SIZE - 1);
    for (i = 0; i < AQ_SIZE - 1; i++) aq_push(&e);
    CHECK(aq_space() == 0);
    aq_clear();
    CHECK(aq_space() == AQ_SIZE - 1);
    /* a stalled consumer never overflows the queue: generation waits */
    start(7);
    for (i = 0; i < 2000; i++) { audio_now++; engine_service(audio_now); }   /* no draining */
    CHECK(aq_space() < AQ_SIZE);
    CHECK(((aq_head - aq_tail) & AQ_MASK) <= AQ_SIZE - 1);
    CHECK(eng.st_qfull > 0);
    /* lookahead is bounded */
    start(0);
    for (i = 0; i < 600; i++) { audio_now++; engine_service(audio_now); play_due(); CHECK((int16_t)(eng.gtime - audio_now) <= LOOKAHEAD); }
    return fail;
}

TEST(t_motifs)
{
    int fail = 0, i;
    Motif m;
    Rng r;
    start(0);
    run_frames(MIN(2));
    CHECK(eng.hcount >= 6);
    CHECK(motif_capture(1, 0xFF, 6) == 6);
    CHECK(proj.motif[1].len == 6);
    CHECK(proj.motif[1].n[0].rel == 0);
    CHECK(proj.motif[1].n[0].dt == 0);
    for (i = 1; i < 6; i++) CHECK(proj.motif[1].n[i].dt >= 1);
    /* the captured relative material reproduces the recent history */
    {
        uint8_t h = hist_recent(0);
        CHECK((uint8_t)(proj.motif[1].base + proj.motif[1].n[5].rel) == eng.hist[h].note);
    }
    memcpy(&m, &proj.motif[1], sizeof(Motif));
    rng_seed(&r, 5);
    for (i = 0; i < 200; i++) {
        uint8_t k;
        motif_mutate(&m, &r);
        CHECK(m.len == 6);
        for (k = 0; k < m.len; k++) { CHECK(m.n[k].dur >= 1); CHECK(m.n[k].rel > -60 && m.n[k].rel < 60); }
    }
    motif_transpose(&m, 5);
    CHECK(m.base == proj.motif[1].base + 5 || m.base == 100);
    m.tscale = 2; motif_rhythm(&m, 1); CHECK(m.tscale == 3);
    motif_rhythm(&m, 0); motif_rhythm(&m, 0); CHECK(m.tscale == 1);
    motif_rhythm(&m, 2); CHECK(m.len == 5);
    motif_rhythm(&m, 3); CHECK(m.len == 6);
    motif_clear(&m); CHECK(m.len == 0);
    /* recurrence weighting: an imprint with high recurrence is recalled */
    {
        int hits = 0;
        proj.motif[0].len = 0; proj.motif[2].len = 0; proj.motif[3].len = 0;
        proj.motif[1].recur = 15;
        memset(eng.amotif, 0, sizeof(eng.amotif));
        for (i = 0; i < 100; i++) if (motif_pick(&r, AG_MEMORY) == &proj.motif[1]) hits++;
        CHECK(hits == 100);
        proj.motif[1].recur = 0;
        CHECK(motif_pick(&r, AG_MEMORY) == 0);
    }
    return fail;
}

TEST(t_imprint_as_agent_source)
{
    /* an imprint assigned to BLOOM becomes BLOOM's origin motif */
    int fail = 0;
    preset_load(8);                               /* AFTERIMAGE ships with one */
    audio_init(); engine_init();
    CHECK(proj.motif[0].source == AG_BLOOM);
    CHECK(eng.ag[AG_BLOOM].mlen == proj.motif[0].len);
    CHECK(eng.ag[AG_BLOOM].motif[1] == (proj.motif[0].n[1].rel * 7) / 12);
    return fail;
}

TEST(t_scene_transitions)
{
    int fail = 0;
    uint8_t k;
    /* TIME: FIRST LIGHT scene 1 lasts 8 x 15 s */
    start(0);
    CHECK(eng.form.scene == 0);
    run_frames(MIN(2) - 200);
    CHECK(eng.form.scene == 0);
    run_frames(400);
    CHECK(eng.form.scene == 1);
    /* NEXT chain + loop back via explicit next */
    run_frames(MIN(12));
    CHECK(eng.form.transitions >= 4);
    /* MANUAL scenes never advance unless FOREVER */
    preset_load(0);
    for (k = 0; k < NUM_SCENES; k++) proj.scene[k].s[SC_TRANS] = TR_MANUAL;
    proj.glob[GP_FOREVER] = 0;
    audio_init(); engine_init(); engine_set_playing(1); player_reset();
    run_frames(MIN(30));
    CHECK(eng.form.scene == 0 && eng.form.transitions == 0);
    form_goto(3, 0);
    CHECK(eng.form.scene == 3);
    /* END stops a non-FOREVER composition gracefully */
    preset_load(0);
    proj.glob[GP_FOREVER] = 0;
    proj.scene[0].s[SC_NEXT] = NX_END; proj.scene[0].s[SC_LEN] = 2;
    audio_init(); engine_init(); engine_set_playing(1); player_reset();
    run_frames(MIN(2));
    CHECK(eng.playing == 0);
    /* ECOLOGY rule: energy above threshold */
    preset_load(0);
    proj.scene[0].s[SC_TRANS] = TR_ECO; proj.scene[0].s[SC_TPARAM] = 0; proj.scene[0].s[SC_LEN] = 60;
    proj.scene[0].s[SC_ENERGY] = 15; proj.eco[EP_BIAS_E] = 15;
    audio_init(); engine_init(); engine_set_playing(1); player_reset();
    run_frames(MIN(3));
    CHECK(eng.form.scene != 0);
    /* PROBABILITY rule eventually fires */
    preset_load(0);
    proj.scene[0].s[SC_TRANS] = TR_PROB; proj.scene[0].s[SC_TPARAM] = 15; proj.scene[0].s[SC_LEN] = 60;
    audio_init(); engine_init(); engine_set_playing(1); player_reset();
    run_frames(MIN(3));
    CHECK(eng.form.scene != 0);
    /* glides: macros move gradually to the new scene values */
    preset_load(0);
    audio_init(); engine_init(); engine_set_playing(1);
    proj.macro[MAC_DENSITY] = 0;
    proj.scene[2].s[SC_MAC] = 15; proj.scene[2].s[SC_GLIDE] = 5;
    form_goto(2, 0);
    run_frames(60 * 3);
    CHECK(proj.macro[MAC_DENSITY] > 0 && proj.macro[MAC_DENSITY] < 15);
    run_frames(60 * 20);
    CHECK(proj.macro[MAC_DENSITY] == 15);
    return fail;
}

TEST(t_save_roundtrip)
{
    int fail = 0;
    Project copy;
    char nm[PROJ_NAME_LEN + 1];
    memset(host_sram, 0xFF, sizeof(host_sram));
    preset_load(3);
    memcpy(&copy, &proj, sizeof(Project));
    CHECK(slot_status(2) == SLOT_EMPTY);
    CHECK(save_slot(2) == 1);
    CHECK(slot_status(2) == SLOT_OK);
    CHECK(slot_name(2, nm) == 1);
    CHECK(memcmp(nm, "DISSOLVING LIGHT", 16) == 0);
    preset_load(0);
    CHECK(memcmp(&copy, &proj, sizeof(Project)) != 0);
    CHECK(load_slot(2) == 1);
    CHECK(memcmp(&copy, &proj, sizeof(Project)) == 0);
    /* second save alternates copies, generation increases */
    proj.seed_lo ^= 0x55;
    CHECK(save_slot(2) == 1);
    preset_load(0);
    CHECK(load_slot(2) == 1);
    CHECK(proj.seed_lo == (uint8_t)(copy.seed_lo ^ 0x55));
    /* other slots untouched */
    CHECK(slot_status(1) == SLOT_EMPTY && slot_status(3) == SLOT_EMPTY);
    /* many saves: generation counter wraps safely */
    {
        int i;
        for (i = 0; i < 600; i++) { proj.seed_hi = (uint8_t)i; CHECK(save_slot(5) == 1); }
        preset_load(0);
        CHECK(load_slot(5) == 1);
        CHECK(proj.seed_hi == (uint8_t)599);
    }
    CHECK(fletcher16((const uint8_t *)"abcde", 5) == 0xC8F0);
    return fail;
}

TEST(t_save_corruption)
{
    int fail = 0, i;
    Project a, b;
    memset(host_sram, 0xFF, sizeof(host_sram));
    /* uninitialised SRAM (random garbage) is never accepted */
    srand(12345);
    for (i = 0; i < 32768; i++) host_sram[i] = (uint8_t)rand();
    for (i = 0; i < NUM_SLOTS; i++) CHECK(slot_status((uint8_t)i) != SLOT_OK);
    for (i = 0; i < NUM_SLOTS; i++) CHECK(load_slot((uint8_t)i) == 0);
    memset(host_sram, 0x00, sizeof(host_sram));
    for (i = 0; i < NUM_SLOTS; i++) CHECK(slot_status((uint8_t)i) == SLOT_EMPTY);
    memset(host_sram, 0xFF, sizeof(host_sram));
    /* save twice (A then B), corrupt the newest copy -> backup is used */
    preset_load(1); memcpy(&a, &proj, sizeof(Project)); save_slot(4);
    preset_load(2); memcpy(&b, &proj, sizeof(Project)); save_slot(4);
    preset_load(0); load_slot(4); CHECK(memcmp(&proj, &b, sizeof(Project)) == 0);
    host_sram[(4 * 2 + 1) * COPY_SIZE + HDR_SIZE + 100] ^= 0x10;     /* copy B payload */
    CHECK(slot_status(4) == SLOT_BACKUP);
    preset_load(0); CHECK(load_slot(4) == 1);
    CHECK(memcmp(&proj, &a, sizeof(Project)) == 0);
    /* header damage is detected too */
    host_sram[(4 * 2 + 0) * COPY_SIZE + 5] ^= 0x01;                   /* copy A generation */
    CHECK(slot_status(4) == SLOT_CORRUPT);
    CHECK(load_slot(4) == 0);
    /* the next save repairs the slot without touching neighbours */
    save_slot(3);
    preset_load(5); CHECK(save_slot(4) == 1);
    CHECK(slot_status(4) == SLOT_OK);
    CHECK(slot_status(3) == SLOT_OK);
    /* interrupted save (magic not yet written) leaves previous version */
    preset_load(6); save_slot(6);
    preset_load(7); save_slot(6);
    host_sram[(6 * 2 + 0) * COPY_SIZE + 0] = 0;  /* pretend copy A's commit never happened */
    preset_load(0); CHECK(load_slot(6) == 1);
    CHECK(memcmp(proj.name, "SOFT MACHINES", 13) == 0);
    slot_erase(6); CHECK(slot_status(6) == SLOT_EMPTY);
    return fail;
}

TEST(t_seed_reproducibility)
{
    int fail = 0;
    uint32_t h1, h2, h3, h4;
    long n1;
    start(2); run_frames(MIN(10)); h1 = log_hash(); n1 = nev;
    start(2); run_frames(MIN(10)); h2 = log_hash();
    CHECK(h1 == h2);
    CHECK(nev == n1);
    preset_load(2); proj.seed_lo ^= 1;
    audio_init(); engine_init(); engine_set_playing(1); player_reset();
    run_frames(MIN(10)); h3 = log_hash();
    CHECK(h3 != h1);
    /* RESTART reproduces even after the project ran for a while */
    start(2); run_frames(MIN(3));
    engine_set_playing(0); engine_restart(); preset_load(2); engine_init(); audio_now = 0; engine_set_playing(1); player_reset();
    run_frames(MIN(10)); h4 = log_hash();
    CHECK(h4 == h1);
    /* reproducible regardless of how irregularly the main loop is serviced */
    {
        long f = 0;
        start(2);
        while (f < MIN(10)) {
            int burst = 1 + (int)(f % 7), k;          /* simulate slow UI frames */
            for (k = 0; k < burst && f < MIN(10); k++, f++) { audio_now++; play_due(); }
            engine_service(audio_now);
            play_due();
        }
        /* events may be late but the decision sequence is identical */
        {
            uint32_t h = 2166136261u;
            long i;
            for (i = 0; i < nev; i++) h = (h ^ evlog[i].note) * 16777619u, h = (h ^ evlog[i].ch) * 16777619u;
            {
                uint32_t g = 2166136261u;
                start(2); run_frames(MIN(10));
                for (i = 0; i < nev; i++) g = (g ^ evlog[i].note) * 16777619u, g = (g ^ evlog[i].ch) * 16777619u;
                CHECK(g == h);
            }
        }
    }
    return fail;
}

TEST(t_long_run_stability)
{
    /* 2 simulated hours per factory composition */
    int fail = 0;
    uint8_t p;
    for (p = 0; p < NUM_PRESETS; p++) {
        long last10, at_ceiling = 0, f;
        uint16_t prev_events;
        start(p);
        if (!proj.glob[GP_FOREVER]) proj.glob[GP_FOREVER] = 1;
        for (f = 0; f < MIN(120); f++) {
            audio_now++; engine_service(audio_now); play_due();
            if ((eng.eco.v[ECO_ENERGY] >> 8) > 250 || (eng.eco.v[ECO_TENSION] >> 8) > 250) at_ceiling++;
            if (f == MIN(110)) last10 = nev;
            if (nev >= MAXEV - 10) nev = 0;
        }
        prev_events = eng.st_events;
        CHECK(eng.playing);
        CHECK(eng.form.hours == 2 || (eng.form.hours == 1 && eng.form.total_s > 3500));
        CHECK(nev - last10 > 5 || nev < last10);       /* still making music at the end */
        CHECK(at_ceiling < MIN(120) / 10);              /* never pinned at saturation */
        CHECK(eng.form.transitions >= 6);               /* long-form development */
        CHECK(type_violation == 0 && overlap_violation == 0);
        CHECK(prev_events > 100);
        CHECK(eng.hs.root < 12 && eng.hs.degree < 7);
    }
    return fail;
}

TEST(t_musical_configurations)
{
    int fail = 0;
    /* 1 sparse melodic generation */
    start(4); run_frames(MIN(30));
    CHECK(nev / 30 >= 5 && nev / 30 <= 60);
    /* 2 dense agent activity (all agents, high density) */
    {
        uint8_t k;
        preset_load(7);
        for (k = 0; k < NUM_SCENES; k++) proj.scene[k].s[SC_MASK] = 0xFF, proj.scene[k].s[SC_MAC] = 15;
        for (k = 0; k < NUM_AGENTS; k++) proj.agent[k].a[AP_EN] = 1, proj.agent[k].a[AP_ACT] = 15;
        audio_init(); engine_init(); engine_set_playing(1); player_reset();
        run_frames(MIN(10));
        CHECK(nev / 10 > 150);
        CHECK(eng.st_qfull == 0);
        CHECK(type_violation == 0 && overlap_violation == 0);
    }
    /* 3 harmonic changes */
    start(3); run_frames(MIN(20)); CHECK(eng.hs.changes >= 5);
    /* 5 motif recall: MEMORY recalls the AFTERIMAGE imprint */
    start(8); run_frames(MIN(20)); CHECK((eng.eco.v[ECO_MEMORY] >> 8) > 150);
    /* 6 erosion: ERODE deletes material only when it is active */
    start(8); run_frames(MIN(30)); CHECK(eng.st_eroded > 0);
    {
        uint8_t k;
        preset_load(8);
        for (k = 0; k < NUM_SCENES; k++) proj.scene[k].s[SC_MASK] &= (uint8_t)~(1 << AG_ERODE);
        audio_init(); engine_init(); engine_set_playing(1); player_reset();
        run_frames(MIN(30));
        CHECK(eng.st_eroded == 0);
    }
    /* 8 noise textures */
    {
        long i, noise = 0;
        start(2); run_frames(MIN(5));
        for (i = 0; i < nev; i++) if (evlog[i].ch == CH_NOISE) noise++;
        CHECK(noise > 50);
    }
    return fail;
}

TEST(t_clocked_timing)
{
    /* CLOCKED: onsets fall on the 16th-note grid of the tempo */
    int fail = 0;
    long i, off = 0, pitched = 0;
    start(7);
    run_frames(MIN(3));
    for (i = 0; i < nev; i++) {
        /* 108 BPM -> 16th = 8.296 frames; phase accumulator steps are 8 or 9 frames apart */
        if (evlog[i].ch == CH_NOISE) continue;
        pitched++;
        if (i > 0 && evlog[i].t != evlog[i - 1].t) {
            uint16_t d = (uint16_t)(evlog[i].t - evlog[i - 1].t);
            if (d < 8) off++;
        }
    }
    CHECK(pitched > 100);
    CHECK(off < pitched / 20);      /* only phrase-player echoes may fall between steps */
    return fail;
}

int main(void)
{
    printf("AETHER - by Synth Centric : host test-suite\n");
    printf("project record %u bytes, engine state %u bytes\n", (unsigned)sizeof(Project), (unsigned)sizeof(Engine));
    RUN(t_rng_reproducible);
    RUN(t_scale_quantisation);
    RUN(t_pitch_choice_in_range_and_scale);
    RUN(t_consonance_preference);
    RUN(t_engine_ranges);
    RUN(t_agent_rules);
    RUN(t_ecology_bounds_and_damping);
    RUN(t_ecology_feedback_changes_music);
    RUN(t_voice_allocation);
    RUN(t_drone_protection);
    RUN(t_event_queue_capacity);
    RUN(t_motifs);
    RUN(t_imprint_as_agent_source);
    RUN(t_scene_transitions);
    RUN(t_save_roundtrip);
    RUN(t_save_corruption);
    RUN(t_seed_reproducibility);
    RUN(t_clocked_timing);
    RUN(t_musical_configurations);
    RUN(t_long_run_stability);
    printf("%d tests, %d checks, %d failed\n", tests_run, checks, tests_failed);
    return tests_failed ? 1 : 0;
}
