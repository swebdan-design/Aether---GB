/*
 * AETHER - by Synth Centric
 * ui_agents.c : AGENTS page - overview of the eight logical agents, a
 * full configuration editor for each, and the ECOLOGY screen.
 */
#pragma bank 255
#include <gb/gb.h>
#include <string.h>
#include "ui.h"
#include "engine.h"

#define LV_TOP 0
#define LV_EDIT 1
#define LV_ECO 2

static uint8_t level, cur, sub_cur, sub_top;

static const char * const agent_names[NUM_AGENTS] = {
    "ORBIT", "BLOOM", "DRIFT", "SCATTER", "TIDE", "ECHO", "ERODE", "MEMORY"
};
static const char * const agent_desc[NUM_AGENTS][2] = {
    { "REPEATING FIGURE", "AROUND THE CENTRE" },
    { "MOTIF GROWS INTO", "PHRASES, RETURNS" },
    { "CONSTRAINED", "RANDOM WALK" },
    { "SPARSE PARTICLES", "AND CLUSTERS" },
    { "SLOW TIDES OF", "ACTIVITY + DRONES" },
    { "RECALLS RECENT", "PHRASES, ALTERED" },
    { "DISSOLVES NOTES,", "MOTIFS, HARMONY" },
    { "LEARNS INTERVALS", "RECALLS MOTIFS" },
};
static const char * const pan_names[] = { "CENTRE", "LEFT", "RIGHT", "ALTERN", "RANDOM" };
static const char * const iset_names[] = { "FIFTHS", "TRIAD", "FOURTHS", "OCTAVES", "STEPS", "CLUSTER", "WIDE", "PENTA" };
static const char * const disp_names[] = { "NONE", "OCT UP", "OCT DOWN", "RANDOM", "FIFTH" };
static const char * const cycle_names[] = { "CYCLE", "HOLD" };

#define COMMON \
    { "ENABLED",   B_AGENT, AP_EN,   0, 1,   F_ONOFF, 0, 0 }, \
    { "ACTIVITY",  B_AGENT, AP_ACT,  0, 15,  F_BAR, 0, 0 }, \
    { "LOW",       B_AGENT, AP_LO,   24, 107, F_NOTE, 0, 0 }, \
    { "HIGH",      B_AGENT, AP_HI,   24, 107, F_NOTE, 0, 0 }, \
    { "INSTR",     B_AGENT, AP_INST, 0, 7,   F_INSTR, 0, 0 }, \
    { "INSTR 2",   B_AGENT, AP_INST2, 0, 8,  F_INSTR, 0, 0 }, \
    { "DUR MIN",   B_AGENT, AP_DMIN, 0, 15,  F_DUR, 0, 0 }, \
    { "DUR MAX",   B_AGENT, AP_DMAX, 0, 15,  F_DUR, 0, 0 }, \
    { "RATE",      B_AGENT, AP_RATE, 0, 15,  F_RATE, 0, 0 }, \
    { "TIMING",    B_AGENT, AP_JIT,  0, 15,  F_NUM, 0, 0 }, \
    { "MUTATION",  B_AGENT, AP_MUT,  0, 15,  F_NUM, 0, 0 }, \
    { "HARMONY",   B_AGENT, AP_HARM, 0, 15,  F_NUM, 0, 0 }, \
    { "ECO SENS",  B_AGENT, AP_ECO,  0, 15,  F_NUM, 0, 0 }, \
    { "PRIORITY",  B_AGENT, AP_PRI,  0, 7,   F_NUM, 0, 0 }, \
    { "STEREO",    B_AGENT, AP_PAN,  0, 4,   F_NAME, 0, pan_names }, \
    { "VELOCITY",  B_AGENT, AP_VOL,  0, 15,  F_VOL, 0, 0 }, \
    { "SEED",      B_AGENT, AP_SEED, 0, 255, F_NUM, 0, 0 }
#define NCOMMON 17

static const PDesc t_orbit[] = { COMMON,
    { "RADIUS",    B_AGENT, AP_P1, 1, 12, F_NUM, 0, 0 },
    { "INTERVALS", B_AGENT, AP_P2, 0, 7, F_NAME, 0, iset_names },
    { "VARIATION", B_AGENT, AP_P3, 0, 15, F_NUM, 0, 0 },
    { "LENGTH",    B_AGENT, AP_P4, 2, 8, F_NUM, 0, 0 } };
static const PDesc t_bloom[] = { COMMON,
    { "MOTIF LEN", B_AGENT, AP_P1, 2, 6, F_NUM, 0, 0 },
    { "BRANCHING", B_AGENT, AP_P2, 0, 15, F_NUM, 0, 0 },
    { "SILENCE",   B_AGENT, AP_P3, 0, 15, F_NUM, 0, 0 },
    { "MAX LEN",   B_AGENT, AP_P4, 4, 16, F_NUM, 0, 0 } };
static const PDesc t_drift[] = { COMMON,
    { "MAX STEP",  B_AGENT, AP_P1, 1, 7, F_NUM, 0, 0 },
    { "BIAS",      B_AGENT, AP_P2, 0, 14, F_SIGNED, 7, 0 },
    { "RETURN",    B_AGENT, AP_P3, 0, 15, F_NUM, 0, 0 },
    { "START",     B_AGENT, AP_P4, 0, 14, F_SIGNED, 7, 0 } };
static const PDesc t_scatter[] = { COMMON,
    { "CLUSTER",   B_AGENT, AP_P1, 0, 15, F_NUM, 0, 0 },
    { "SILENCE",   B_AGENT, AP_P2, 0, 15, F_NUM, 0, 0 },
    { "DUR VAR",   B_AGENT, AP_P3, 0, 15, F_NUM, 0, 0 },
    { "TIMBRES",   B_AGENT, AP_P4, 0, 3, F_NUM, -1, 0 } };
static const PDesc t_tide[] = { COMMON,
    { "PERIOD",    B_AGENT, AP_P1, 1, 15, F_LEN30, 0, 0 },
    { "DEPTH",     B_AGENT, AP_P2, 0, 15, F_NUM, 0, 0 },
    { "DRONES",    B_AGENT, AP_P3, 0, 15, F_NUM, 0, 0 } };
static const PDesc t_echo[] = { COMMON,
    { "DELAY",     B_AGENT, AP_P1, 0, 15, F_RATE, 0, 0 },
    { "DISPLACE",  B_AGENT, AP_P2, 0, 4, F_NAME, 0, disp_names },
    { "FRAGMENT",  B_AGENT, AP_P3, 0, 15, F_NUM, 0, 0 },
    { "INTERVAL",  B_AGENT, AP_P4, 0, 15, F_NUM, 0, 0 } };
static const PDesc t_erode[] = { COMMON,
    { "STRENGTH",  B_AGENT, AP_P1, 0, 15, F_NUM, 0, 0 },
    { "SPEED",     B_AGENT, AP_P2, 0, 15, F_NUM, 0, 0 },
    { "MOTIF CUT", B_AGENT, AP_P3, 0, 15, F_NUM, 0, 0 },
    { "MODE",      B_AGENT, AP_P4, 0, 1, F_NAME, 0, cycle_names } };
static const PDesc t_memory[] = { COMMON,
    { "RECALL",    B_AGENT, AP_P1, 0, 15, F_NUM, 0, 0 },
    { "FOCUS",     B_AGENT, AP_P2, 0, 15, F_NUM, 0, 0 },
    { "LEARNING",  B_AGENT, AP_P3, 0, 15, F_NUM, 0, 0 } };

static const PDesc * const agent_tabs[NUM_AGENTS] = {
    t_orbit, t_bloom, t_drift, t_scatter, t_tide, t_echo, t_erode, t_memory
};
static const uint8_t agent_ntab[NUM_AGENTS] = {
    NCOMMON + 4, NCOMMON + 4, NCOMMON + 4, NCOMMON + 4, NCOMMON + 3, NCOMMON + 4, NCOMMON + 4, NCOMMON + 3
};

static const PDesc eco_tab[] = {
    { "INFL E",    B_ECO, EP_INF_E, 0, 15, F_BAR, 0, 0 },
    { "INFL T",    B_ECO, EP_INF_T, 0, 15, F_BAR, 0, 0 },
    { "INFL S",    B_ECO, EP_INF_S, 0, 15, F_BAR, 0, 0 },
    { "INFL M",    B_ECO, EP_INF_M, 0, 15, F_BAR, 0, 0 },
    { "BIAS E",    B_ECO, EP_BIAS_E, 0, 15, F_SIGNED, 8, 0 },
    { "BIAS T",    B_ECO, EP_BIAS_T, 0, 15, F_SIGNED, 8, 0 },
    { "BIAS S",    B_ECO, EP_BIAS_S, 0, 15, F_SIGNED, 8, 0 },
    { "BIAS M",    B_ECO, EP_BIAS_M, 0, 15, F_SIGNED, 8, 0 },
    { "DAMPING",   B_ECO, EP_DAMP, 0, 15, F_NUM, 0, 0 },
    { "COUPLING",  B_ECO, EP_COUPLE, 0, 15, F_NUM, 0, 0 },
    { "FREEZE",    B_ECO, EP_FREEZE, 0, 1, F_ONOFF, 0, 0 },
};
#define N_ECO (sizeof(eco_tab) / sizeof(PDesc))

/* ------------------------------------------------------------- overview */
static uint8_t act_dot(uint8_t i);
static void top_row(uint8_t i)
{
    uint8_t y = (uint8_t)(2 + i);
    uint8_t sel = cur == i;
    put_tile(0, y, sel ? T_CURSOR : 0);
    if (i == NUM_AGENTS) {
        y++;
        put_tile(0, y, sel ? T_CURSOR : 0);
        put_tile(0, (uint8_t)(y - 1), 0);
        put_low(3, y, "ecology", 9);
        return;
    }
    put_tile(1, y, proj.agent[i].a[AP_EN] ? T_BOX_ON : T_BOX_OFF);
    put_low(3, y, agent_names[i], 8);
    put_tile(11, y, act_dot(i));
    memcpy(lb, proj.inst[proj.agent[i].a[AP_INST] & 7].name, INST_NAME_LEN); lb[6] = 0;
    put_low(13, y, lb, 7);
}

static uint8_t act_dot(uint8_t i)
{
    if (!((eng.form.live.s[SC_MASK] >> i) & 1) || !proj.agent[i].a[AP_EN]) return 0;
    return (eng.ag[i].fired && eng.playing) ? T_DOT_ON : T_DOTSM;
}

static void top_desc(void)
{
    if (cur < NUM_AGENTS) ui_help2(agent_desc[cur][0], agent_desc[cur][1]);
    else ui_help("ECOLOGY");
}

static void draw_top(void)
{
    uint8_t i;
    gfx_cls();
    ui_header(0);
    for (i = 0; i <= NUM_AGENTS; i++) top_row(i);
    put_low(1, 13, "l/r on-off  a edit", 19);
    put_low(1, 14, "dot: playing now", 19);
    top_desc();
}

/* ----------------------------------------------------------------- edit */
static void draw_edit(void)
{
    gfx_cls();
    ui_header(agent_names[ui_agent]);
    list_draw(agent_tabs[ui_agent], agent_ntab[ui_agent], 2, 13, sub_cur, sub_top);
}

/* -------------------------------------------------------------- ecology */
static void eco_bars(void)
{
    static const char * const n[4] = { "energy", "tension", "stability", "memory" };
    uint8_t i;
    for (i = 0; i < 4; i++) {
        put_low(1, (uint8_t)(2 + i), n[i], 9);
        put_hbar(10, (uint8_t)(2 + i), 6, ECO8(i), 255);
        put_num(17, (uint8_t)(2 + i), ECO8(i), 3);
    }
    put_low(1, 6, eng.eco.resolving ? "resolving tension" : (eng.eco.silent > 4 ? "silence settling " : "                 "), 18);
}

static void draw_eco(void)
{
    gfx_cls();
    ui_header("ecology");
    eco_bars();
    list_draw(eco_tab, N_ECO, 8, 6, sub_cur, sub_top);
}

void pg_agents(uint8_t keys, uint8_t mode) BANKED
{
    if (mode == M_ENTER) { level = LV_TOP; mode = M_DRAW; }
    if (mode == M_DRAW) {
        if (level == LV_EDIT) draw_edit();
        else if (level == LV_ECO) draw_eco();
        else draw_top();
        return;
    }
    if (mode == M_TICK) {
        if (level == LV_TOP && (ui_frame_cnt & 3) == 0) {
            uint8_t i = (uint8_t)((ui_frame_cnt >> 2) & 7);
            put_tile(11, (uint8_t)(2 + i), act_dot(i));
        } else if (level == LV_ECO && (ui_frame_cnt & 7) == 0) eco_bars();
        return;
    }
    switch (level) {
    case LV_TOP: {
        uint8_t old = cur;
        if ((keys & K_UP) && cur > 0) cur--;
        if ((keys & K_DOWN) && cur < NUM_AGENTS) cur++;
        if (old != cur) { top_row(old); top_row(cur); top_desc(); }
        if (keys & K_B) { ui_home(); return; }
        if ((keys & (K_LEFT | K_RIGHT)) && cur < NUM_AGENTS) {
            proj.agent[cur].a[AP_EN] ^= 1;
            top_row(cur);
        }
        if ((keys & (K_LEFT | K_RIGHT)) && cur < NUM_AGENTS)
            ui_msg(proj.agent[cur].a[AP_EN] ? "AGENT ON" : "AGENT OFF");
        if (keys & K_A) {
            sub_cur = 0; sub_top = 0;
            if (cur < NUM_AGENTS) { ui_agent = cur; level = LV_EDIT; draw_edit(); }
            else { level = LV_ECO; draw_eco(); }
        }
        break; }
    case LV_EDIT: {
        uint8_t r = list_run(agent_tabs[ui_agent], agent_ntab[ui_agent], 2, 13, &sub_cur, &sub_top, keys);
        if (r == LI_BACK) { level = LV_TOP; draw_top(); }
        else if (r == LI_CHANGED) {
            /* structural parameters rebuild the agent's working material */
            if (sub_cur >= NCOMMON || sub_cur == NCOMMON - 1) agent_init(ui_agent);
        }
        break; }
    default:
        {
            uint8_t r = list_run(eco_tab, N_ECO, 8, 6, &sub_cur, &sub_top, keys);
            if (r == LI_BACK) { level = LV_TOP; draw_top(); }
            else if (r == LI_CHANGED) form_eco_targets();
        }
        break;
    }
}
