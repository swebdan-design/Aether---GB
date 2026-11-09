/*
 * AETHER - by Synth Centric
 * engine.h : runtime state and API of the generative core
 * (agents, ecology, harmony, modulation, form, imprint, voice allocation).
 *
 * The core is platform independent.  On the Game Boy it lives in banked
 * ROM and is driven from the main loop; on the host it is driven directly
 * by the test-suite.
 */
#ifndef AETHER_ENGINE_H
#define AETHER_ENGINE_H

#include "aether.h"

/* -------------------------------------------------------------- tuning */
#define LOOKAHEAD      6     /* frames the generator runs ahead of playback */
#define MAX_CATCHUP    8     /* generator ticks per service call            */
#define HIST_N        32     /* event history ring                          */
#define NUM_PHRASE     3     /* concurrent phrase players                   */
#define MARKOV_N      13     /* interval classes -6..+6                     */
#define VIZ_N          8
#define REST         (-128)

/* ----------------------------------------------------------------- rng */
typedef struct { uint16_t s; } Rng;
void     rng_seed(Rng *r, uint16_t seed) NONBANKED;
uint8_t  rng8(Rng *r) NONBANKED;
uint16_t rng16(Rng *r) NONBANKED;
uint8_t  rng_below(Rng *r, uint8_t n) NONBANKED;      /* 0..n-1      */
uint8_t  rng_chance(Rng *r, uint8_t p) NONBANKED;     /* p/256       */
int8_t   rng_spread(Rng *r, uint8_t n) NONBANKED;     /* -n..+n      */

/* ------------------------------------------------------- runtime state */
typedef struct {
    Rng      rng;
    uint16_t timer;       /* frames (FREE) or steps (CLOCKED) to next decision */
    uint16_t hold;        /* frames of scene-entry stagger                     */
    uint8_t  last;        /* last emitted note                                 */
    int8_t   last_iv;     /* last interval                                     */
    uint8_t  centre;      /* working centre note                               */
    uint8_t  pos, len, phase, cycles, mlen;
    int8_t   pat[16];     /* pattern / phrase buffer                           */
    uint8_t  pdur[16];    /* per element duration index                        */
    int8_t   motif[8];    /* BLOOM origin motif                                */
    uint8_t  mdur[8];     /* BLOOM origin rhythm                               */
    uint8_t  cluster;     /* SCATTER remaining cluster events                  */
    uint8_t  fired;       /* UI flash countdown                                */
    uint8_t  alt;         /* ALT panning toggle / misc                         */
} AgentState;

typedef struct {
    uint16_t until;       /* virtual time the channel becomes free  */
    uint16_t start;       /* trigger time                           */
    uint8_t  prio;
    uint8_t  agent;
    uint8_t  note;
    uint8_t  protect;     /* sustained note protected from stealing */
} VChan;

typedef struct {
    uint16_t time;
    uint8_t  note;
    uint8_t  dur4;
    uint8_t  agent;
    uint8_t  ch;
} HistEv;

typedef struct {
    uint8_t  active, agent, inst, pos, len, vol, prio, pan, tscale, base;
    uint8_t  fragment;    /* probability (0-255) of dropping a note */
    uint16_t next;
    MotifNote n[MOTIF_MAX];
} Phrase;

typedef struct {
    uint16_t v[4];        /* 8.8 fixed point, ENERGY TENSION STABILITY MEMORY */
    int16_t  acc[4];      /* impulses gathered since last update              */
    uint8_t  target[4];   /* baselines 0-255 (scene targets + bias)           */
    uint8_t  silent;      /* consecutive updates without notes                */
    uint8_t  resolving;   /* tension resolution in progress                   */
    uint8_t  notes;       /* notes during the last period                     */
} Ecology;

#define ECO_ENERGY  0
#define ECO_TENSION 1
#define ECO_STAB    2
#define ECO_MEMORY  3

typedef struct {
    uint8_t  root;        /* current tonal centre (pc)            */
    uint8_t  home;        /* scene / project tonal centre         */
    uint8_t  scale;
    uint16_t mask;        /* absolute pitch-class mask             */
    uint16_t chord;       /* absolute pitch classes of chord       */
    uint8_t  degree;      /* current chord degree                  */
    uint8_t  pc_hist[12];
    uint8_t  iv_hist[12];
    uint8_t  recent[16];
    uint8_t  rpos;
    uint8_t  centres[4];
    uint8_t  bars;
    uint8_t  simplify;    /* 0-3 erosion driven simplification      */
    uint8_t  croot;       /* pitch class of the current chord root  */
    uint8_t  pc_ok[12];   /* cached: pitch class in the active set  */
    uint8_t  pc_cons[12]; /* cached: consonance 0-15 vs the chord   */
    uint8_t  changes;     /* number of harmonic changes (stats)     */
} HarmonyState;

typedef struct {
    uint16_t phase;
    int8_t   val;
    int8_t   tgt;
    uint8_t  cnt;
} ModRt;

typedef struct {
    uint8_t  scene;
    uint16_t elapsed;     /* seconds in scene  */
    uint16_t total_s;     /* seconds since start (wraps after 18h) */
    uint8_t  hours;
    uint16_t sec_acc;     /* frame accumulator (8.8) */
    uint16_t clk;         /* clock phase accumulator */
    uint8_t  glide;       /* glide seconds remaining */
    uint8_t  bar_steps;   /* clocked: steps in current bar */
    uint8_t  tempo_tgt;
    uint8_t  mac_tgt[NUM_MACROS];
    uint8_t  ending;      /* fade-out in progress  */
    uint8_t  transitions; /* number of scene changes */
    uint8_t  forever_gen;
    uint8_t  pending;     /* scheduled manual scene (0xFF none) */
    Scene    live;        /* runtime copy (FOREVER mode mutates this) */
} FormState;

typedef struct {
    uint8_t note, ch, agent;
} VizEv;

typedef struct {
    uint8_t   playing;
    uint8_t   step;           /* 1 when a clock step occurs this tick */
    uint16_t  gtime;          /* generation time in frames (wraps)    */
    uint32_t  ticks;
    Rng       rng;
    AgentState ag[NUM_AGENTS];
    VChan     vc[NUM_CH];
    HistEv    hist[HIST_N];
    uint8_t   hpos, hcount;
    Phrase    ph[NUM_PHRASE];
    Ecology   eco;
    HarmonyState hs;
    ModRt     mrt[NUM_MODS];
    int8_t    mod[NUM_MDEST];
    FormState form;
    uint8_t   tide;           /* TIDE level 0-255 */
    uint16_t  tide_phase;
    uint8_t   tide_tgt;
    uint8_t   erosion;        /* ERODE level 0-255 */
    uint8_t   tex_timer, tex_pos;
    uint8_t   tex_act;        /* noise texture slow activity 0-255 */
    Motif     amotif[NUM_AUTO_MOTIFS];
    uint8_t   apos;
    uint8_t   markov[MARKOV_N][MARKOV_N];
    VizEv     viz[VIZ_N];
    uint8_t   viz_w, viz_r;
    uint8_t   ch_agent[NUM_CH];
    uint8_t   stage_k;            /* next wave staging buffer */
    /* statistics */
    uint16_t  st_events, st_dropped, st_stolen, st_eroded, st_qfull;
    uint8_t   st_maxbehind;
    uint16_t  st_ag_ev[10];       /* per source: agents 0-7, texture 8, user 9 */
    uint16_t  st_ag_drop[10];
} Engine;

extern Engine eng;

/* x * f / 128 for f <= 255 using only 16-bit arithmetic (x <= 4095);
 * SDCC's 32-bit multiply is far too slow for the per-decision paths */
uint16_t scale128(uint16_t x, uint8_t f) NONBANKED;

/* duration / interval tables (frames) live in engine.c */
uint16_t dur_frames(uint8_t idx) BANKED;
uint16_t rate_frames(uint8_t idx) BANKED;
uint16_t rate_steps(uint8_t idx) BANKED;

/* ------------------------------------------------------------- engine */
void    engine_init(void) BANKED;            /* project loaded -> full restart */
void    engine_restart(void) BANKED;         /* reset runtime from seed        */
void    engine_set_playing(uint8_t on) BANKED;
void    engine_service(uint16_t now) BANKED; /* main-loop entry                */
void    engine_tick(void) BANKED;            /* one generated frame            */
void    engine_stop_all(void) BANKED;
uint8_t engine_audition(uint8_t inst, uint8_t note, uint8_t dur) BANKED;

/* emit a note request (returns channel or 0xFF if dropped) */
#define EMF_NOERODE 1   /* bypass erosion filter      */
#define EMF_PROTECT 2   /* protect from stealing      */
#define EMF_RECALL  4   /* recalled material (memory) */
uint8_t emit_note(uint8_t agent, uint8_t inst, uint8_t note, uint8_t dur_idx_or_frames4,
                  uint8_t vol, uint8_t prio, uint8_t flags) BANKED;
uint8_t voice_alloc(uint8_t itype, uint8_t prio, uint16_t dur, uint8_t prefer_ch1) BANKED;
void    phrase_start(const MotifNote *n, uint8_t len, uint8_t base, uint8_t agent,
                     uint8_t inst, uint8_t vol, uint8_t tscale, uint16_t delay,
                     uint8_t fragment) BANKED;
uint8_t hist_recent(uint8_t back) BANKED;   /* index into eng.hist, 0 = newest */
#define HIST_IDX(back) ((uint8_t)((eng.hpos + HIST_N - 1 - (back)) & (HIST_N - 1)))

/* -------------------------------------------------------------- agents */
void    agents_reset(void) BANKED;
void    agent_init(uint8_t i) BANKED;     /* re-initialise one agent (e.g. new imprint source) */
void    agents_tick(void) BANKED;
void    agents_second(void) BANKED;
void    agent_on_scene(uint8_t old_mask, uint8_t new_mask, uint8_t glide) BANKED;
void    markov_learn(int8_t prev_iv, int8_t iv) BANKED;

/* ------------------------------------------------------------- harmony */
void    harm_reset(void) BANKED;
void    harm_refresh(void) BANKED;           /* settings changed            */
void    harm_bar(void) BANKED;               /* meso-time harmonic step     */
uint8_t harm_choose(uint8_t target, uint8_t lo, uint8_t hi, uint8_t prev,
                    uint8_t adherence, uint8_t radius, Rng *r) BANKED;
uint8_t harm_quantize(uint8_t note) BANKED;
uint8_t harm_degree(uint8_t from, int8_t steps) BANKED;  /* move n scale steps */
uint8_t harm_centre(uint8_t lo, uint8_t hi) BANKED;      /* tonal centre in range */
void    harm_note(uint8_t note, uint8_t prev) BANKED;    /* memory update  */
uint8_t harm_consonance(uint8_t note) BANKED;            /* 0-15 vs chord  */
uint16_t scale_mask(uint8_t scale) BANKED;

/* ------------------------------------------------------------- ecology */
void    eco_reset(void) BANKED;
void    eco_update(void) BANKED;
uint8_t eco_get(uint8_t v) BANKED;                       /* 0-255           */
int8_t  eco_infl(uint8_t v, uint8_t sens) BANKED;        /* -64..+64 signed influence */
/* inline form for hot paths (same arithmetic as eco_infl) */
#define ECO_INFL(x, sens) ((int8_t)((((int16_t)(eng.eco.v[(x)] >> 8) - 128) * \
                           (int16_t)(uint8_t)(proj.eco[EP_INF_E + (x)] * (sens))) >> 9))
#define ECO_IMPULSE(x, amt) (eng.eco.acc[(x)] += (amt))
#define ECO8(x) ((uint8_t)(eng.eco.v[(x)] >> 8))

/* ---------------------------------------------------------- modulation */
void    mod_reset(void) BANKED;
void    mod_update(void) BANKED;
int8_t  mod_source(uint8_t i) BANKED;

/* --------------------------------------------------- synthesis helpers */
#define WT_SINE 0
#define WT_TRI  1
#define WT_SAW  2
#define WT_SQUARE 3
#define WT_ORGAN 4
#define WT_HOLLOW 5
#define WT_GLASS 6
#define WT_REED 7
#define WT_PULSE25 8
#define WT_FORMANT 9
#define NUM_WTEMPL 10
#define WX_RANDOM 0
#define WX_MUTATE 1
#define WX_SMOOTH 2
#define WX_INVERT 3
#define WX_REVERSE 4
#define WX_SHIFT 5
#define WX_FOLD 6
#define WX_CRUSH 7
#define WX_AMP 8
#define WX_SOFTEN 9
#define NUM_WXFORM 10
void    wave_template(uint8_t slot, uint8_t kind) BANKED;
void    wave_transform(uint8_t slot, uint8_t op, Rng *r) BANKED;
void    wave_sync_live(void) BANKED;
uint8_t wave_get(uint8_t slot, uint8_t i) BANKED;
void    wave_set(uint8_t slot, uint8_t i, uint8_t v) BANKED;
void    wave_evolve(uint8_t slot, uint8_t amount, Rng *r) BANKED;
void    wave_blend(uint8_t a, uint8_t b, uint8_t amt, uint8_t *out) BANKED;
#define NUM_FACTORY_PATCHES 18
void    patch_factory(uint8_t slot, uint8_t idx) BANKED;
void    patch_factory_name(uint8_t idx, char *out) BANKED;

/* ---------------------------------------------------------------- form */
void    form_reset(void) BANKED;
void    form_tick(void) BANKED;                 /* clock + seconds          */
void    form_goto(uint8_t scene, uint8_t immediate) BANKED;
void    form_store_scene(uint8_t scene) BANKED; /* snapshot current state   */
void    form_eco_targets(void) BANKED;      /* recompute ecology baselines */

/* --------------------------------------------------------------- motif */
uint8_t motif_capture(uint8_t slot, uint8_t agent_filter, uint8_t count) BANKED;
void    motif_mutate(Motif *m, Rng *r) BANKED;
void    motif_transpose(Motif *m, int8_t semis) BANKED;
void    motif_rhythm(Motif *m, uint8_t expand) BANKED;
void    motif_play(Motif *m, uint8_t agent, uint16_t delay) BANKED;
void    motif_clear(Motif *m) BANKED;
void    motif_auto_capture(uint8_t agent) BANKED;
Motif  *motif_pick(Rng *r, uint8_t agent) BANKED;  /* weighted by recurrence */

/* ------------------------------------------------------------- presets */
#define NUM_PRESETS 9      /* 0 = first-run default, 1..8 factory compositions */
void    preset_load(uint8_t idx) BANKED;
void    preset_name(uint8_t idx, char *out) BANKED;

#endif
