/*
 * AETHER - by Synth Centric
 * factory.c : the default first-run composition and the eight factory
 * compositions.  Every composition is a complete behavioural system
 * (instruments, waves, agents, harmony, ecology, modulation, scenes),
 * not a recorded note sequence.
 */
#pragma bank 255
#include <string.h>
#include "engine.h"

/* agent rows:  EN ACT LO  HI INST I2 DMN DMX MUT RT JIT HRM ECO PRI SEED PAN VOL P1 P2 P3 P4 */
static const uint8_t base_agents[NUM_AGENTS][AP_COUNT] = {
    { 1,  9, 60, 79, 0, 8,  5,  8,  4,  8,  3, 13,  8,  4, 11, 0, 0,  7, 0,  4,  5 }, /* ORBIT   */
    { 1, 10, 62, 84, 1, 0,  4,  9,  5,  7,  4, 12,  8,  4, 23, 3, 0,  3, 5,  4, 12 }, /* BLOOM   */
    { 1,  8, 55, 76, 3, 1,  6, 10,  4,  9,  6, 12,  8,  3, 37, 0, 0,  2, 7,  5,  7 }, /* DRIFT   */
    { 1,  6, 72, 96, 2, 7,  1,  5,  6,  7, 10, 11, 10,  2, 41, 4, 0,  6, 7,  6,  0 }, /* SCATTER */
    { 1,  8, 36, 55, 4, 8, 13, 15,  3, 11,  4, 14,  8,  6, 53, 0, 0,  6, 10, 8,  0 }, /* TIDE    */
    { 1,  7, 48, 96, 1, 0,  4,  8,  6,  8,  5, 12,  8,  2, 67, 3, 0,  8, 1,  4,  3 }, /* ECHO    */
    { 1,  6,  0,127, 0, 8,  0,  0,  0, 12,  4,  0,  8,  0, 71, 0, 0, 10, 4,  6,  0 }, /* ERODE   */
    { 1,  6, 57, 84, 0, 1,  5,  9,  5, 10,  6, 12,  8,  3, 89, 0, 0,  5, 6,  6,  0 }, /* MEMORY  */
};

/* scene rows: MASK ROOT SCALE  D  E  H  M  T  S  TM TEMPO  EN TE ST ME LEN TR TP NX GL HM ON */
static const uint8_t base_scenes[5][SC_COUNT] = {
    { 0x19, 12, 10,  3, 5, 3, 4, 5, 12,  2,  0,  3, 3, 10, 6,  8, TR_TIME, 0, NX_NEXT, 4, 3, 1 },
    { 0x97, 12, 10,  6, 7, 5, 6, 5,  9,  2,  0,  6, 5,  8, 6, 12, TR_TIME, 0, NX_NEXT, 5, 3, 1 },
    { 0xBF, 12, 10, 10,10, 8, 9, 8,  6,  2,  0, 10, 9,  6, 8, 12, TR_TIME, 0, NX_NEXT, 6, 3, 1 },
    { 0x7D, 12, 10,  5, 8, 7, 4, 6, 12,  2,  0,  4, 6,  5, 8, 10, TR_TIME, 0, NX_NEXT, 6, 3, 1 },
    { 0xB3, 12, 10,  6, 6, 4, 5, 4, 10,  2,  0,  5, 3, 11,13, 12, TR_TIME, 0, 0,       5, 3, 1 },
};
static const char base_scene_names[5][SCENE_NAME_LEN + 1] = {
    "ARRIVAL ", "EMERGE  ", "EXPAND  ", "DISSOLVE", "RETURN  "
};

static const char preset_names[NUM_PRESETS][PROJ_NAME_LEN + 1] = {
    "FIRST LIGHT",
    "PALE TRANSMISSION",
    "STATIC MEMORY",
    "DISSOLVING LIGHT",
    "EMPTY ROOMS",
    "GLASS ORBIT",
    "NIGHT CURRENT",
    "SOFT MACHINES",
    "AFTERIMAGE",
};

static const uint8_t default_iw[12] = { 15, 2, 9, 11, 12, 11, 3, 14, 10, 11, 7, 3 };

static void set_name(char *dst, const char *src, uint8_t n)
{
    uint8_t i = 0;
    for (; i < n && src[i]; i++) dst[i] = src[i];
    for (; i < n; i++) dst[i] = ' ';
}

static void ag(uint8_t i, uint8_t k, uint8_t v) { proj.agent[i].a[k] = v; }

static void agent_row(uint8_t i, uint8_t en, uint8_t act, uint8_t lo, uint8_t hi, uint8_t inst, uint8_t rate)
{
    ag(i, AP_EN, en); ag(i, AP_ACT, act); ag(i, AP_LO, lo); ag(i, AP_HI, hi);
    ag(i, AP_INST, inst); ag(i, AP_RATE, rate);
}

static void mod(uint8_t k, uint8_t src, uint8_t dest, int8_t depth, uint8_t rate)
{
    proj.mod[k].src = src; proj.mod[k].dest = dest;
    proj.mod[k].depth = (uint8_t)(depth + 15); proj.mod[k].rate = rate;
}

static void scene(uint8_t k, const char *nm, uint8_t mask, uint8_t d, uint8_t e, uint8_t h,
                  uint8_t m, uint8_t t, uint8_t s, uint8_t en, uint8_t te, uint8_t st, uint8_t me,
                  uint8_t len, uint8_t next)
{
    Scene *sc = &proj.scene[k];
    sc->s[SC_MASK] = mask; sc->s[SC_ROOT] = 12; sc->s[SC_SCALE] = 10;
    sc->s[SC_MAC + 0] = d; sc->s[SC_MAC + 1] = e; sc->s[SC_MAC + 2] = h;
    sc->s[SC_MAC + 3] = m; sc->s[SC_MAC + 4] = t; sc->s[SC_MAC + 5] = s;
    sc->s[SC_TMODE] = 2; sc->s[SC_TEMPO] = 0;
    sc->s[SC_ENERGY] = en; sc->s[SC_TENSION] = te; sc->s[SC_STAB] = st; sc->s[SC_MEM] = me;
    sc->s[SC_LEN] = len; sc->s[SC_TRANS] = TR_TIME; sc->s[SC_TPARAM] = 0;
    sc->s[SC_NEXT] = next; sc->s[SC_GLIDE] = 5; sc->s[SC_HMODE] = 3; sc->s[SC_ON] = 1;
    set_name(sc->name, nm, SCENE_NAME_LEN);
}

static void clear_scenes_from(uint8_t k)
{
    for (; k < NUM_SCENES; k++) {
        memset(&proj.scene[k], 0, sizeof(Scene));
        proj.scene[k].s[SC_ROOT] = 12; proj.scene[k].s[SC_SCALE] = 10;
        proj.scene[k].s[SC_TMODE] = 2; proj.scene[k].s[SC_HMODE] = 3;
        proj.scene[k].s[SC_LEN] = 8; proj.scene[k].s[SC_NEXT] = NX_NEXT;
        proj.scene[k].s[SC_GLIDE] = 4;
        { uint8_t j; for (j = 0; j < NUM_MACROS; j++) proj.scene[k].s[SC_MAC + j] = 16; }
        set_name(proj.scene[k].name, "EMPTY", SCENE_NAME_LEN);
    }
}

static void base_project(void)
{
    uint8_t i;
    memset(&proj, 0, sizeof(Project));
    set_name(proj.name, "UNTITLED", PROJ_NAME_LEN);
    proj.seed_lo = 0xED; proj.seed_hi = 0x5E;
    proj.macro[0] = 6; proj.macro[1] = 6; proj.macro[2] = 5;
    proj.macro[3] = 5; proj.macro[4] = 5; proj.macro[5] = 9;

    proj.harm[HP_ROOT] = 2; proj.harm[HP_SCALE] = SCALE_MAJOR;
    proj.harm[HP_LO] = 36; proj.harm[HP_HI] = 100;
    proj.harm[HP_CONS] = 10; proj.harm[HP_DISS] = 2; proj.harm[HP_CHANGE] = 5;
    proj.harm[HP_CSTAB] = 9; proj.harm[HP_RESOL] = 8; proj.harm[HP_REGSEP] = 3;
    proj.harm[HP_MMODE] = MM_REINFORCE; proj.harm[HP_MSTR] = 6;
    proj.harm[HP_UMASKL] = 0x95; proj.harm[HP_UMASKH] = 0x02;      /* major pentatonic */
    for (i = 0; i < 12; i++) proj.harm[HP_IW + i] = default_iw[i];

    for (i = 0; i < 4; i++) { proj.eco[EP_INF_E + i] = 10; proj.eco[EP_BIAS_E + i] = 8; }
    proj.eco[EP_DAMP] = 7; proj.eco[EP_COUPLE] = 8; proj.eco[EP_FREEZE] = 0;

    proj.glob[GP_TMODE] = TM_FREE; proj.glob[GP_TEMPO] = 72; proj.glob[GP_FOREVER] = 1;
    proj.glob[GP_START] = 0; proj.glob[GP_TEXINST] = 6; proj.glob[GP_MVOL] = 7;
    proj.glob[GP_SPREAD] = 6;

    patch_factory(0, 0);   /* GLASS */
    patch_factory(1, 1);   /* HALO  */
    patch_factory(2, 2);   /* BELL  */
    patch_factory(3, 4);   /* PAD   */
    patch_factory(4, 8);   /* DRONE */
    patch_factory(5, 9);   /* WARM  */
    patch_factory(6, 12);  /* RAIN  */
    patch_factory(7, 16);  /* SPARK */
    wave_template(0, WT_SINE);
    wave_template(1, WT_ORGAN);
    wave_template(2, WT_GLASS);
    wave_template(3, WT_TRI);

    memcpy(proj.agent, base_agents, sizeof(base_agents));
    mod(0, MS_LFO, MD_WAVE, 8, 2);
    mod(1, MS_DRIFT, MD_NOTESEL, 4, 3);
    mod(2, MS_OFF, MD_PITCH, 0, 0);
    mod(3, MS_OFF, MD_PITCH, 0, 0);

    for (i = 0; i < 5; i++) {
        memcpy(proj.scene[i].s, base_scenes[i], SC_COUNT);
        memcpy(proj.scene[i].name, base_scene_names[i], SCENE_NAME_LEN);
    }
    clear_scenes_from(5);
    for (i = 0; i < NUM_MOTIFS; i++) motif_clear(&proj.motif[i]);
}

static void key(uint8_t root, uint8_t scale) { proj.harm[HP_ROOT] = root; proj.harm[HP_SCALE] = scale; }
static void macros(uint8_t d, uint8_t e, uint8_t h, uint8_t m, uint8_t t, uint8_t s)
{
    proj.macro[0] = d; proj.macro[1] = e; proj.macro[2] = h;
    proj.macro[3] = m; proj.macro[4] = t; proj.macro[5] = s;
}

/* ---------------------------------------------------------- compositions */
static void p_pale(void)
{
    /* ethereal drones and distant high notes */
    key(4, SCALE_LYDIAN);
    proj.seed_lo = 0x11; proj.seed_hi = 0xA7;
    patch_factory(0, 2);  patch_factory(1, 17); patch_factory(4, 8); patch_factory(5, 11);
    patch_factory(6, 13);                                   /* WIND texture */
    wave_template(0, WT_SINE); wave_template(1, WT_HOLLOW); wave_template(2, WT_GLASS);
    proj.inst[4].p[IP_LEN] = 15; proj.inst[4].p[IP_MOD] = 5;
    agent_row(AG_ORBIT, 0, 6, 64, 84, 1, 10);
    agent_row(AG_BLOOM, 0, 6, 64, 84, 1, 9);
    agent_row(AG_DRIFT, 1, 9, 76, 96, 0, 9); ag(AG_DRIFT, AP_DMIN, 3); ag(AG_DRIFT, AP_DMAX, 7);
    agent_row(AG_SCATTER, 1, 9, 84, 100, 0, 8); ag(AG_SCATTER, AP_P2, 7); ag(AG_SCATTER, AP_INST2, 5);
    agent_row(AG_TIDE, 1, 12, 38, 52, 4, 9); ag(AG_TIDE, AP_P3, 13); ag(AG_TIDE, AP_P1, 8);
    agent_row(AG_ECHO, 1, 8, 72, 100, 0, 8); ag(AG_ECHO, AP_P2, 1); ag(AG_ECHO, AP_P3, 6);
    agent_row(AG_MEMORY, 0, 5, 70, 90, 0, 11);
    proj.harm[HP_CONS] = 12; proj.harm[HP_DISS] = 1; proj.harm[HP_CHANGE] = 3;
    proj.glob[GP_TEXINST] = 6; proj.glob[GP_SPREAD] = 10;
    macros(4, 4, 4, 3, 5, 13);
    mod(0, MS_LFO, MD_WAVE, 12, 1);
    mod(1, MS_TIDE, MD_NOISE, 6, 0);
    mod(2, MS_DRIFT, MD_PAN, 10, 4);
    scene(0, "DUSK", 0x18, 3, 4, 3, 3, 4, 13, 3, 3, 11, 6, 10, NX_NEXT);
    scene(1, "SIGNAL", 0x3C, 5, 5, 5, 4, 6, 12, 6, 5, 8, 8, 14, NX_NEXT);
    scene(2, "CARRIER", 0x1C, 4, 6, 6, 3, 8, 14, 4, 6, 9, 9, 12, 0);
    clear_scenes_from(3);
}

static void p_static(void)
{
    /* unstable tones and electrical textures */
    key(1, SCALE_PHRYGIAN);
    proj.seed_lo = 0x3C; proj.seed_hi = 0x0F;
    patch_factory(0, 6); patch_factory(1, 7); patch_factory(2, 5);
    patch_factory(4, 11); patch_factory(6, 15); patch_factory(7, 16);
    wave_template(2, WT_FORMANT); wave_transform(2, WX_CRUSH, &eng.rng);
    proj.inst[4].p[IP_WSLOT] = 2; proj.inst[4].p[IP_MOD] = 9;
    agent_row(AG_ORBIT, 0, 6, 60, 76, 0, 8);
    agent_row(AG_BLOOM, 0, 6, 60, 80, 1, 8);
    agent_row(AG_DRIFT, 1, 9, 50, 74, 1, 7); ag(AG_DRIFT, AP_P1, 4); ag(AG_DRIFT, AP_JIT, 12); ag(AG_DRIFT, AP_HARM, 8);
    agent_row(AG_SCATTER, 1, 10, 60, 100, 7, 5); ag(AG_SCATTER, AP_P1, 11); ag(AG_SCATTER, AP_P4, 1);
    ag(AG_SCATTER, AP_INST2, 0); ag(AG_SCATTER, AP_JIT, 14);
    agent_row(AG_TIDE, 1, 6, 40, 58, 4, 10); ag(AG_TIDE, AP_P1, 3); ag(AG_TIDE, AP_P2, 14);
    agent_row(AG_ECHO, 1, 7, 50, 90, 0, 7); ag(AG_ECHO, AP_P4, 9); ag(AG_ECHO, AP_P3, 8);
    agent_row(AG_ERODE, 1, 8, 0, 127, 0, 10); ag(AG_ERODE, AP_P1, 9); ag(AG_ERODE, AP_P2, 6);
    agent_row(AG_MEMORY, 1, 9, 52, 80, 2, 8); ag(AG_MEMORY, AP_P1, 9); ag(AG_MEMORY, AP_P2, 12);
    proj.harm[HP_CONS] = 5; proj.harm[HP_DISS] = 8; proj.harm[HP_CHANGE] = 8; proj.harm[HP_CSTAB] = 4;
    proj.harm[HP_MMODE] = MM_EXPLORE;
    proj.eco[EP_BIAS_T] = 12; proj.eco[EP_COUPLE] = 12;
    proj.glob[GP_TEXINST] = 6;
    macros(8, 10, 10, 8, 11, 5);
    mod(0, MS_SH, MD_DUTY, 12, 9);
    mod(1, MS_RWALK, MD_PITCH, 3, 10);
    mod(2, MS_TENSION, MD_NOISE, 10, 0);
    mod(3, MS_LFO, MD_DENSITY, 8, 6);
    scene(0, "HUM", 0x34, 5, 8, 8, 6, 9, 6, 6, 9, 5, 6, 8, NX_NEXT);
    scene(1, "SURGE", 0xEC, 10, 11, 11, 10, 12, 4, 11, 12, 3, 8, 8, NX_NEXT);
    scene(2, "FAULT", 0xE4, 6, 12, 13, 6, 14, 8, 6, 14, 2, 10, 6, NX_RANDOM);
    scene(3, "RESIDUE", 0xA4, 4, 8, 6, 5, 8, 10, 4, 6, 7, 12, 8, 0);
    proj.scene[2].s[SC_TRANS] = TR_ECO; proj.scene[2].s[SC_TPARAM] = 2;
    clear_scenes_from(4);
}

static void p_dissolving(void)
{
    /* slowly evolving consonant harmony */
    key(7, SCALE_MAJOR);
    proj.seed_lo = 0x77; proj.seed_hi = 0x21;
    patch_factory(0, 4); patch_factory(1, 1); patch_factory(2, 17);
    patch_factory(4, 9); patch_factory(5, 8);
    wave_template(0, WT_SINE); wave_template(1, WT_ORGAN);
    proj.inst[4].p[IP_WSLOT] = 1; proj.inst[4].p[IP_WMORPH] = 0; proj.inst[4].p[IP_WAMT] = 3;
    agent_row(AG_ORBIT, 1, 10, 55, 72, 0, 10); ag(AG_ORBIT, AP_P2, 1); ag(AG_ORBIT, AP_DMIN, 9); ag(AG_ORBIT, AP_DMAX, 12);
    agent_row(AG_BLOOM, 1, 11, 62, 81, 1, 9); ag(AG_BLOOM, AP_DMIN, 8); ag(AG_BLOOM, AP_DMAX, 11); ag(AG_BLOOM, AP_P3, 2);
    agent_row(AG_DRIFT, 1, 6, 67, 86, 2, 11); ag(AG_DRIFT, AP_P1, 1); ag(AG_DRIFT, AP_HARM, 15);
    agent_row(AG_SCATTER, 0, 4, 79, 96, 2, 10);
    agent_row(AG_TIDE, 1, 10, 38, 55, 4, 10); ag(AG_TIDE, AP_P1, 10); ag(AG_TIDE, AP_P3, 12);
    agent_row(AG_ECHO, 1, 5, 60, 90, 1, 9);
    agent_row(AG_ERODE, 1, 5, 0, 127, 0, 13); ag(AG_ERODE, AP_P1, 6); ag(AG_ERODE, AP_P2, 2);
    agent_row(AG_MEMORY, 0, 5, 60, 80, 0, 10);
    proj.harm[HP_CONS] = 14; proj.harm[HP_DISS] = 0; proj.harm[HP_CHANGE] = 7; proj.harm[HP_CSTAB] = 12;
    proj.harm[HP_REGSEP] = 4; proj.harm[HP_RESOL] = 12;
    proj.glob[GP_TEXINST] = 8;
    macros(5, 6, 4, 3, 3, 10);
    mod(0, MS_LFO, MD_WAVE, 10, 1);
    mod(1, MS_STAB, MD_LENGTH, 10, 0);
    scene(0, "DAWN", 0x13, 4, 5, 3, 3, 3, 11, 4, 2, 11, 6, 12, NX_NEXT);
    scene(1, "BLOOM", 0x37, 6, 7, 5, 4, 4, 9, 7, 4, 9, 8, 16, NX_NEXT);
    scene(2, "HALATION", 0x37, 7, 8, 6, 5, 4, 8, 8, 5, 8, 9, 14, NX_NEXT);
    scene(3, "DISSOLVE", 0x51, 3, 6, 3, 2, 2, 13, 3, 2, 10, 10, 12, 0);
    clear_scenes_from(4);
}

static void p_empty(void)
{
    /* sparse melodic fragments and extensive silence */
    key(9, SCALE_MINOR);
    proj.seed_lo = 0x09; proj.seed_hi = 0xE0;
    patch_factory(0, 0); patch_factory(1, 4); patch_factory(2, 6);
    patch_factory(4, 9); patch_factory(6, 14);
    agent_row(AG_ORBIT, 0, 5, 60, 76, 0, 12);
    agent_row(AG_BLOOM, 1, 11, 57, 79, 0, 9); ag(AG_BLOOM, AP_P3, 8); ag(AG_BLOOM, AP_JIT, 10);
    agent_row(AG_DRIFT, 1, 9, 50, 70, 1, 10); ag(AG_DRIFT, AP_JIT, 12);
    agent_row(AG_SCATTER, 1, 6, 72, 92, 2, 10); ag(AG_SCATTER, AP_P2, 9);
    agent_row(AG_TIDE, 1, 8, 40, 52, 4, 11); ag(AG_TIDE, AP_P3, 7);
    agent_row(AG_ECHO, 1, 9, 48, 92, 0, 8); ag(AG_ECHO, AP_P1, 10); ag(AG_ECHO, AP_P2, 3); ag(AG_ECHO, AP_P3, 7);
    agent_row(AG_MEMORY, 1, 7, 55, 76, 0, 11);
    proj.harm[HP_CONS] = 11; proj.harm[HP_DISS] = 3; proj.harm[HP_CHANGE] = 2;
    proj.glob[GP_TEXINST] = 6; proj.glob[GP_SPREAD] = 12;
    macros(4, 4, 4, 3, 3, 12);
    mod(0, MS_RWALK, MD_PAN, 12, 4);
    scene(0, "HALLWAY", 0xA6, 3, 4, 4, 3, 2, 14, 2, 4, 9, 7, 12, NX_NEXT);
    scene(1, "WINDOW", 0xAF, 4, 5, 5, 4, 3, 13, 4, 5, 8, 9, 12, NX_NEXT);
    scene(2, "VACANT", 0x26, 2, 4, 3, 2, 2, 15, 1, 3, 10, 11, 10, 0);
    clear_scenes_from(3);
}

static void p_glass(void)
{
    /* bright repeating tones and gradual mutation */
    key(0, SCALE_MAJPENT);
    proj.seed_lo = 0xC1; proj.seed_hi = 0x45;
    patch_factory(0, 0); patch_factory(1, 3); patch_factory(2, 5); patch_factory(3, 2);
    patch_factory(4, 11); patch_factory(6, 16);
    proj.inst[0].p[IP_MOD] = 4;
    proj.glob[GP_TMODE] = TM_CLOCKED; proj.glob[GP_TEMPO] = 96;
    agent_row(AG_ORBIT, 1, 12, 67, 86, 0, 2); ag(AG_ORBIT, AP_P2, 6); ag(AG_ORBIT, AP_P4, 6); ag(AG_ORBIT, AP_P3, 3);
    ag(AG_ORBIT, AP_DMIN, 3); ag(AG_ORBIT, AP_DMAX, 6); ag(AG_ORBIT, AP_JIT, 0); ag(AG_ORBIT, AP_PAN, 3);
    agent_row(AG_BLOOM, 1, 8, 72, 91, 3, 5); ag(AG_BLOOM, AP_JIT, 0);
    agent_row(AG_DRIFT, 0, 6, 60, 79, 1, 4);
    agent_row(AG_SCATTER, 1, 6, 84, 100, 2, 6); ag(AG_SCATTER, AP_P4, 1); ag(AG_SCATTER, AP_JIT, 0);
    agent_row(AG_TIDE, 1, 6, 43, 55, 4, 7);
    agent_row(AG_ECHO, 1, 7, 60, 96, 1, 7); ag(AG_ECHO, AP_P1, 6); ag(AG_ECHO, AP_P2, 4);
    agent_row(AG_MEMORY, 0, 4, 60, 84, 0, 4);
    proj.harm[HP_CONS] = 12; proj.harm[HP_DISS] = 1; proj.harm[HP_CHANGE] = 4;
    proj.glob[GP_TEXINST] = 8;
    macros(9, 6, 4, 7, 6, 6);
    mod(0, MS_LFO, MD_DUTY, 15, 3);
    mod(1, MS_STEP, MD_NOTESEL, 6, 2);
    scene(0, "FACET", 0x31, 8, 5, 3, 6, 5, 6, 7, 3, 10, 6, 8, NX_NEXT);
    scene(1, "PRISM", 0x3B, 10, 7, 5, 8, 7, 6, 9, 5, 8, 8, 12, NX_NEXT);
    scene(2, "REFRACT", 0x2B, 9, 9, 6, 9, 8, 7, 9, 7, 6, 9, 10, 0);
    clear_scenes_from(3);
}

static void p_night(void)
{
    /* low pulses and unpredictable rhythmic particles */
    key(2, SCALE_DORIAN);
    proj.seed_lo = 0x4E; proj.seed_hi = 0x8B;
    patch_factory(0, 4); patch_factory(1, 6); patch_factory(4, 10); patch_factory(5, 9);
    patch_factory(6, 12); patch_factory(7, 14);
    proj.inst[0].p[IP_SHAPE] = 2; proj.inst[0].p[IP_ENVPACE] = 2; proj.inst[0].p[IP_LEN] = 6;
    proj.inst[6].p[IP_PATTERN] = 0x92;
    proj.glob[GP_TMODE] = TM_CLOCKED; proj.glob[GP_TEMPO] = 68;
    agent_row(AG_ORBIT, 1, 10, 38, 50, 0, 3); ag(AG_ORBIT, AP_P2, 3); ag(AG_ORBIT, AP_P4, 7); ag(AG_ORBIT, AP_JIT, 6);
    agent_row(AG_BLOOM, 0, 6, 55, 72, 1, 5);
    agent_row(AG_DRIFT, 1, 8, 45, 62, 0, 4); ag(AG_DRIFT, AP_JIT, 10); ag(AG_DRIFT, AP_INST2, 5);
    agent_row(AG_SCATTER, 1, 9, 60, 96, 7, 2); ag(AG_SCATTER, AP_P1, 10); ag(AG_SCATTER, AP_JIT, 12);
    ag(AG_SCATTER, AP_INST2, 1);
    agent_row(AG_TIDE, 1, 9, 26, 40, 4, 8); ag(AG_TIDE, AP_P3, 10);
    agent_row(AG_ECHO, 1, 7, 45, 80, 1, 4); ag(AG_ECHO, AP_P1, 5); ag(AG_ECHO, AP_P3, 6);
    agent_row(AG_MEMORY, 1, 5, 45, 66, 0, 6);
    proj.harm[HP_LO] = 26;
    proj.harm[HP_CONS] = 9; proj.harm[HP_DISS] = 4; proj.harm[HP_CHANGE] = 5;
    proj.glob[GP_TEXINST] = 6;
    macros(7, 6, 6, 9, 9, 7);
    mod(0, MS_SH, MD_NOISE, 10, 7);
    mod(1, MS_TIDE, MD_DENSITY, 8, 0);
    scene(0, "UNDERTOW", 0x35, 6, 5, 5, 8, 8, 8, 6, 5, 8, 6, 10, NX_NEXT);
    scene(1, "CURRENT", 0xBD, 9, 7, 6, 11, 10, 6, 9, 7, 6, 8, 14, NX_NEXT);
    scene(2, "DEPTH", 0x39, 5, 6, 7, 6, 11, 9, 5, 7, 7, 9, 10, 0);
    clear_scenes_from(3);
}

static void p_soft(void)
{
    /* interacting minimalist sequences: cycles of 5, 6 and 7 phase */
    key(5, SCALE_MIXO);
    proj.seed_lo = 0x5A; proj.seed_hi = 0x3D;
    patch_factory(0, 0); patch_factory(1, 7); patch_factory(2, 6); patch_factory(3, 4);
    patch_factory(4, 9); patch_factory(6, 15);
    proj.inst[6].p[IP_PATTERN] = 0x88; proj.inst[6].p[IP_DENS] = 5;
    proj.glob[GP_TMODE] = TM_CLOCKED; proj.glob[GP_TEMPO] = 108;
    agent_row(AG_ORBIT, 1, 13, 65, 79, 0, 3); ag(AG_ORBIT, AP_P4, 5); ag(AG_ORBIT, AP_P2, 4); ag(AG_ORBIT, AP_P3, 2);
    ag(AG_ORBIT, AP_JIT, 0); ag(AG_ORBIT, AP_DMIN, 2); ag(AG_ORBIT, AP_DMAX, 4); ag(AG_ORBIT, AP_PAN, 1);
    agent_row(AG_BLOOM, 1, 12, 60, 76, 1, 3); ag(AG_BLOOM, AP_P1, 3); ag(AG_BLOOM, AP_P4, 6); ag(AG_BLOOM, AP_P3, 0);
    ag(AG_BLOOM, AP_JIT, 0); ag(AG_BLOOM, AP_PAN, 2); ag(AG_BLOOM, AP_DMIN, 3); ag(AG_BLOOM, AP_DMAX, 5);
    agent_row(AG_DRIFT, 1, 10, 53, 67, 4, 5); ag(AG_DRIFT, AP_P1, 1); ag(AG_DRIFT, AP_JIT, 0); ag(AG_DRIFT, AP_HARM, 15);
    agent_row(AG_SCATTER, 0, 4, 79, 96, 2, 6);
    agent_row(AG_TIDE, 1, 4, 41, 53, 4, 9); ag(AG_TIDE, AP_P3, 2); ag(AG_TIDE, AP_P1, 5);
    agent_row(AG_ECHO, 0, 5, 60, 90, 1, 6);
    agent_row(AG_MEMORY, 1, 9, 62, 79, 3, 7); ag(AG_MEMORY, AP_P1, 3); ag(AG_MEMORY, AP_JIT, 0);
    proj.harm[HP_CONS] = 12; proj.harm[HP_DISS] = 1; proj.harm[HP_CHANGE] = 3; proj.harm[HP_REGSEP] = 2;
    proj.glob[GP_TEXINST] = 6;
    macros(10, 4, 4, 6, 4, 4);
    mod(0, MS_LFO, MD_DUTY, 10, 2);
    scene(0, "IDLE", 0x13, 9, 3, 3, 5, 3, 4, 7, 3, 11, 6, 8, NX_NEXT);
    scene(1, "MESH", 0x97, 11, 4, 4, 6, 4, 4, 9, 4, 10, 8, 14, NX_NEXT);
    scene(2, "SHIFT", 0x95, 10, 7, 6, 7, 5, 5, 9, 6, 8, 9, 12, NX_NEXT);
    scene(3, "REST", 0x11, 6, 3, 2, 4, 2, 8, 4, 2, 12, 9, 8, 0);
    clear_scenes_from(4);
}

static void p_afterimage(void)
{
    /* motif memory, recurrence and gradual erosion */
    static const int8_t rel[5] = { 0, 3, 7, 5, 10 };
    static const uint8_t dt[5] = { 0, 10, 10, 20, 15 };
    static const uint8_t du[5] = { 8, 8, 16, 10, 30 };
    uint8_t k;
    key(11, SCALE_MINOR);
    proj.seed_lo = 0xAF; proj.seed_hi = 0x7E;
    patch_factory(0, 0); patch_factory(1, 1); patch_factory(2, 2);
    patch_factory(4, 9); patch_factory(6, 14);
    agent_row(AG_ORBIT, 0, 6, 60, 76, 0, 10);
    agent_row(AG_BLOOM, 1, 8, 62, 81, 0, 9);
    agent_row(AG_DRIFT, 1, 5, 55, 74, 1, 11);
    agent_row(AG_SCATTER, 0, 4, 76, 96, 2, 9);
    agent_row(AG_TIDE, 1, 7, 38, 52, 4, 11); ag(AG_TIDE, AP_P3, 8);
    agent_row(AG_ECHO, 1, 10, 55, 90, 1, 7); ag(AG_ECHO, AP_P1, 9); ag(AG_ECHO, AP_P3, 8); ag(AG_ECHO, AP_P2, 3);
    agent_row(AG_ERODE, 1, 8, 0, 127, 0, 9); ag(AG_ERODE, AP_P1, 13); ag(AG_ERODE, AP_P2, 3); ag(AG_ERODE, AP_P3, 9);
    agent_row(AG_MEMORY, 1, 10, 57, 81, 0, 8); ag(AG_MEMORY, AP_P1, 11); ag(AG_MEMORY, AP_P3, 10);
    proj.harm[HP_MMODE] = MM_REINFORCE; proj.harm[HP_MSTR] = 12;
    proj.eco[EP_BIAS_M] = 12;
    proj.glob[GP_TEXINST] = 6;
    /* a remembered phrase, recurring and feeding BLOOM */
    motif_clear(&proj.motif[0]);
    proj.motif[0].len = 5; proj.motif[0].base = 66; proj.motif[0].inst = 0;
    proj.motif[0].tscale = 2; proj.motif[0].recur = 12; proj.motif[0].source = AG_BLOOM;
    for (k = 0; k < 5; k++) { proj.motif[0].n[k].rel = rel[k]; proj.motif[0].n[k].dt = dt[k]; proj.motif[0].n[k].dur = du[k]; }
    macros(5, 6, 5, 4, 4, 11);
    mod(0, MS_MEMORY, MD_LENGTH, 8, 0);
    mod(1, MS_LFO, MD_WAVE, 9, 2);
    scene(0, "IMPRESS", 0xB6, 6, 5, 4, 4, 3, 9, 6, 4, 9, 8, 10, NX_NEXT);
    scene(1, "RECALL", 0xB7, 7, 6, 5, 5, 4, 9, 7, 5, 8, 12, 12, NX_NEXT);
    scene(2, "FADING", 0xF6, 4, 7, 4, 3, 4, 12, 4, 4, 6, 12, 12, NX_NEXT);
    scene(3, "AFTERIMG", 0xE0, 3, 5, 3, 2, 3, 14, 2, 3, 9, 14, 10, 0);
    clear_scenes_from(4);
}

void preset_name(uint8_t idx, char *out) BANKED
{
    uint8_t i;
    if (idx >= NUM_PRESETS) idx = 0;
    for (i = 0; i < PROJ_NAME_LEN && preset_names[idx][i]; i++) out[i] = preset_names[idx][i];
    out[i] = 0;
}

void preset_load(uint8_t idx) BANKED
{
    if (idx >= NUM_PRESETS) idx = 0;
    base_project();
    switch (idx) {
    case 1: p_pale(); break;
    case 2: p_static(); break;
    case 3: p_dissolving(); break;
    case 4: p_empty(); break;
    case 5: p_glass(); break;
    case 6: p_night(); break;
    case 7: p_soft(); break;
    case 8: p_afterimage(); break;
    default: break;
    }
    set_name(proj.name, preset_names[idx], PROJ_NAME_LEN);
}
