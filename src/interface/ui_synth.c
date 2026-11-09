/*
 * AETHER - by Synth Centric
 * ui_synth.c : SYNTH page.
 *   instrument slots -> PULSE LAB / WAVE LAB parameters / NOISE LAB
 *   WAVE LAB waveform editor (32 x 4-bit, drawn with the D-pad)
 *   MODULATION matrix, OUTPUT (channel roles, texture, master, stereo)
 */
#pragma bank 255
#include <gb/gb.h>
#include <string.h>
#include "ui.h"
#include "engine.h"
#include "audio.h"

#define LV_TOP  0
#define LV_INST 1
#define LV_WAVE 2
#define LV_MOD  3
#define LV_OUT  4

static uint8_t level, cur, top, sub_cur, sub_top, naming, npos;
static uint8_t wslot, wpos, wdraw, wtempl, wxform;

static const char * const itype_names[] = { "PULSE", "WAVE", "NOISE" };
static const char * const patch_names[NUM_FACTORY_PATCHES] = {
    "GLASS", "HALO", "BELL", "CHIME", "PAD", "ARPEG", "MOTE", "REED", "DRONE",
    "WARM", "SUB", "CRYSTL", "RAIN", "WIND", "DUST", "STATIC", "SPARK", "SWELL"
};
static const char * const duty_names[] = { "12.5%", "25%", "50%", "75%" };
static const char * const env_names[] = { "DECAY", "SWELL" };
static const char * const fade_names[] = { "OUT", "IN" };
static const char * const arp_names[] = { "OFF", "OCTAVE", "FIFTH", "MAJOR", "MINOR7", "SUS", "SPREAD", "2 OCT" };
static const char * const sweep_names[] = { "UP", "DOWN" };
static const char * const retrig_names[] = { "LEGATO", "ALWAYS" };
static const char * const level_names[] = { "25%", "50%", "100%" };
static const char * const width_names[] = { "15 BIT", "7 BIT" };
static const char * const act_name[] = { "a: edit" };
static const char * const rename_name[] = { "a: name" };

#define COMMON_HEAD \
    { "NAME",      B_NONE, 0, 0, 0, F_ACTION, 0, rename_name }, \
    { "TYPE",      B_INST, IP_TYPE, 0, 2, F_NAME, 0, itype_names }, \
    { "PRESET",    B_UI, 0, 0, NUM_FACTORY_PATCHES - 1, F_NAME, 0, patch_names }

static const PDesc pulse_tab[] = {
    COMMON_HEAD,
    { "DUTY",      B_INST, IP_SHAPE, 0, 3, F_NAME, 0, duty_names },
    { "VOLUME",    B_INST, IP_VOL, 0, 15, F_BAR, 0, 0 },
    { "ENVELOPE",  B_INST, IP_ENVDIR, 0, 1, F_NAME, 0, env_names },
    { "ENV PACE",  B_INST, IP_ENVPACE, 0, 7, F_NUM, 0, 0 },
    { "LENGTH",    B_INST, IP_LEN, 1, 15, F_NUM, 0, 0 },
    { "VIB DEPTH", B_INST, IP_VIBD, 0, 15, F_NUM, 0, 0 },
    { "VIB RATE",  B_INST, IP_VIBR, 0, 15, F_NUM, 0, 0 },
    { "VIB DELAY", B_INST, IP_VIBDLY, 0, 15, F_NUM, 0, 0 },
    { "ARPEGGIO",  B_INST, IP_ARP, 0, 7, F_NAME, 0, arp_names },
    { "ARP SPEED", B_INST, IP_ARPSPD, 1, 15, F_NUM, 0, 0 },
    { "PITCH ENV", B_INST, IP_PENV, 0, 24, F_SIGNED, 12, 0 },
    { "PENV SPD",  B_INST, IP_PENVSPD, 0, 15, F_NUM, 0, 0 },
    { "SWEEP",     B_INST, IP_SWPACE, 0, 7, F_NUM, 0, 0 },
    { "SWEEP DIR", B_INST, IP_SWDIR, 0, 1, F_NAME, 0, sweep_names },
    { "SWP STEP",  B_INST, IP_SWSTEP, 0, 7, F_NUM, 0, 0 },
    { "RETRIGGER", B_INST, IP_RETRIG, 0, 1, F_NAME, 0, retrig_names },
    { "RANDOM",    B_INST, IP_RAND, 0, 15, F_NUM, 0, 0 },
    { "OCTAVE",    B_INST, IP_OCT, 0, 4, F_SIGNED, 2, 0 },
    { "PWM RATE",  B_INST, IP_MOD, 0, 15, F_NUM, 0, 0 },
};
static const PDesc wave_tab[] = {
    COMMON_HEAD,
    { "WAVE A",    B_INST, IP_WSLOT, 0, 3, F_NUM, -1, 0 },
    { "WAVE B",    B_INST, IP_WMORPH, 0, 3, F_NUM, -1, 0 },
    { "MORPH",     B_INST, IP_WAMT, 0, 8, F_NUM, 0, 0 },
    { "LEVEL",     B_INST, IP_VOL, 1, 3, F_NAME, 0, level_names },
    { "FADE",      B_INST, IP_ENVDIR, 0, 1, F_NAME, 0, fade_names },
    { "FADE PACE", B_INST, IP_ENVPACE, 0, 7, F_NUM, 0, 0 },
    { "LENGTH",    B_INST, IP_LEN, 1, 15, F_NUM, 0, 0 },
    { "VIB DEPTH", B_INST, IP_VIBD, 0, 15, F_NUM, 0, 0 },
    { "VIB RATE",  B_INST, IP_VIBR, 0, 15, F_NUM, 0, 0 },
    { "VIB DELAY", B_INST, IP_VIBDLY, 0, 15, F_NUM, 0, 0 },
    { "ARPEGGIO",  B_INST, IP_ARP, 0, 7, F_NAME, 0, arp_names },
    { "ARP SPEED", B_INST, IP_ARPSPD, 1, 15, F_NUM, 0, 0 },
    { "PITCH ENV", B_INST, IP_PENV, 0, 24, F_SIGNED, 12, 0 },
    { "PENV SPD",  B_INST, IP_PENVSPD, 0, 15, F_NUM, 0, 0 },
    { "RETRIGGER", B_INST, IP_RETRIG, 0, 1, F_NAME, 0, retrig_names },
    { "RANDOM",    B_INST, IP_RAND, 0, 15, F_NUM, 0, 0 },
    { "OCTAVE",    B_INST, IP_OCT, 0, 4, F_SIGNED, 2, 0 },
    { "EVOLVE",    B_INST, IP_MOD, 0, 15, F_NUM, 0, 0 },
};
static const PDesc noise_tab[] = {
    COMMON_HEAD,
    { "WIDTH",     B_INST, IP_NWIDTH, 0, 1, F_NAME, 0, width_names },
    { "VOLUME",    B_INST, IP_VOL, 0, 15, F_BAR, 0, 0 },
    { "ENVELOPE",  B_INST, IP_ENVDIR, 0, 1, F_NAME, 0, env_names },
    { "ENV PACE",  B_INST, IP_ENVPACE, 0, 7, F_NUM, 0, 0 },
    { "LENGTH",    B_INST, IP_LEN, 1, 15, F_NUM, 0, 0 },
    { "PITCH",     B_INST, IP_NBASE, 0, 23, F_NUM, 0, 0 },
    { "SPREAD",    B_INST, IP_NSPREAD, 0, 7, F_NUM, 0, 0 },
    { "DRIFT",     B_INST, IP_NDRIFTD, 0, 15, F_NUM, 0, 0 },
    { "DRIFT SPD", B_INST, IP_NDRIFTR, 0, 15, F_NUM, 0, 0 },
    { "DENSITY",   B_INST, IP_DENS, 0, 15, F_BAR, 0, 0 },
    { "IRREGULAR", B_INST, IP_IRREG, 0, 15, F_NUM, 0, 0 },
    { "PATTERN",   B_INST, IP_PATTERN, 0, 255, F_BITS, 0, 0 },
    { "ACTIVITY",  B_INST, IP_MOD, 0, 15, F_NUM, 0, 0 },
    { "RETRIGGER", B_INST, IP_RETRIG, 0, 1, F_NAME, 0, retrig_names },
};
#define N_PULSE (sizeof(pulse_tab) / sizeof(PDesc))
#define N_WAVE  (sizeof(wave_tab) / sizeof(PDesc))
#define N_NOISE (sizeof(noise_tab) / sizeof(PDesc))

static const char * const src_names[NUM_MSRC] = {
    "OFF", "LFO", "RND WALK", "S&H", "ENVELOPE", "DRIFT", "STEP", "SCENE",
    "ENERGY", "TENSION", "STABILITY", "MEMORY", "TIDE"
};
static const char * const dst_names[NUM_MDEST] = {
    "PITCH", "NOTE SEL", "DENSITY", "WAVE", "DUTY", "LENGTH", "NOISE", "VOLUME", "STEREO", "MUTATION"
};
#define MODROWS(n, o) \
    { n " SOURCE", B_MOD, o + 0, 0, NUM_MSRC - 1, F_NAME, 0, src_names }, \
    { n " DEST",   B_MOD, o + 1, 0, NUM_MDEST - 1, F_NAME, 0, dst_names }, \
    { n " DEPTH",  B_MOD, o + 2, 0, 30, F_SIGNED, 15, 0 }, \
    { n " RATE",   B_MOD, o + 3, 0, 15, F_NUM, 0, 0 }
static const PDesc mod_tab[] = { MODROWS("1", 0), MODROWS("2", 4), MODROWS("3", 8), MODROWS("4", 12) };

static const char * const role_names[] = { "AUTO", "DRONE", "MELODY", "PARTICLE", "OFF" };
static const PDesc out_tab[] = {
    { "CH1 PULSE", B_GLOB, GP_ROLE0 + 0, 0, 4, F_NAME, 0, role_names },
    { "CH2 PULSE", B_GLOB, GP_ROLE0 + 1, 0, 4, F_NAME, 0, role_names },
    { "CH3 WAVE",  B_GLOB, GP_ROLE0 + 2, 0, 4, F_NAME, 0, role_names },
    { "CH4 NOISE", B_GLOB, GP_ROLE0 + 3, 0, 4, F_NAME, 0, role_names },
    { "TEXTURE",   B_GLOB, GP_TEXINST, 0, 8, F_INSTR, 0, 0 },
    { "MASTER",    B_GLOB, GP_MVOL, 0, 7, F_NUM, 0, 0 },
    { "STEREO",    B_GLOB, GP_SPREAD, 0, 15, F_BAR, 0, 0 },
};
#define N_MOD (sizeof(mod_tab) / sizeof(PDesc))
#define N_OUT (sizeof(out_tab) / sizeof(PDesc))

static const char * const wt_names[NUM_WTEMPL] = {
    "SINE", "TRIANGLE", "SAW", "SQUARE", "ORGAN", "HOLLOW", "GLASS", "REED", "PULSE 25", "FORMANT"
};
static const char * const wx_names[NUM_WXFORM] = {
    "RANDOM", "MUTATE", "SMOOTH", "INVERT", "REVERSE", "SHIFT", "FOLD", "CRUSH", "NORMAL", "SOFTEN"
};

static const PDesc *inst_tab(uint8_t *n)
{
    uint8_t t = proj.inst[ui_inst].p[IP_TYPE];
    if (t == IT_WAVE) { *n = N_WAVE; return wave_tab; }
    if (t == IT_NOISE) { *n = N_NOISE; return noise_tab; }
    *n = N_PULSE; return pulse_tab;
}

static void audition(void)
{
    const uint8_t *p = proj.inst[ui_inst].p;
    uint8_t note = p[IP_TYPE] == IT_NOISE ? p[IP_NBASE] : 60;
    uint8_t d4 = p[IP_TYPE] == IT_NOISE ? (uint8_t)(4 + p[IP_LEN] * 2) : 24;
    if (engine_audition(ui_inst, note, d4) == 0xFF) ui_msg("CHANNEL BUSY");
}

/* --------------------------------------------------------------- top */
static const char * const menu_items[3] = { "WAVE LAB", "MODULATION", "OUTPUT" };
static void top_row(uint8_t i)
{
    uint8_t y = (uint8_t)(2 + i + (i >= NUM_INST ? 1 : 0));
    uint8_t sel = cur == i;
    put_tile(0, y, sel ? T_CURSOR : 0);
    if (i < NUM_INST) {
        lb[0] = (char)('1' + i); lb[1] = ' ';
        memcpy(lb + 2, proj.inst[i].name, INST_NAME_LEN);
        lb[8] = 0;
        put_low(1, y, lb, 8);
        put_low(10, y, itype_names[proj.inst[i].p[IP_TYPE] % 3], 6);
        put_low(16, y, i == proj.glob[GP_TEXINST] ? "tex" : "   ", 3);
    } else {
        put_low(1, y, menu_items[i - NUM_INST], 12);
    }
}

static void top_help(void)
{
    if (cur < NUM_INST) {
        static const char * const th[3] = { "pulse: channel 1/2", "wave: channel 3", "noise: channel 4" };
        ui_help2("a: edit  l/r: listen", th[proj.inst[cur].p[IP_TYPE] % 3]);
    } else ui_help(menu_items[cur - NUM_INST]);
}

static void draw_top(void)
{
    uint8_t i;
    gfx_cls();
    ui_header(0);
    for (i = 0; i < NUM_INST + 3; i++) top_row(i);
    top_help();
}

/* --------------------------------------------------------- inst edit */
static void draw_inst(void)
{
    uint8_t n;
    const PDesc *t = inst_tab(&n);
    gfx_cls();
    lb[0] = (char)('1' + ui_inst); lb[1] = ' ';
    memcpy(lb + 2, proj.inst[ui_inst].name, INST_NAME_LEN); lb[8] = 0;
    ui_header(lb);
    list_draw(t, n, 2, 13, sub_cur, sub_top);
}

/* ---------------------------------------------------------- wave lab */
static void wave_render_sample(uint8_t i)
{
    uint8_t v = wave_get(wslot, i);
    uint8_t topy = (uint8_t)(30 - v * 2);
    uint8_t tx = (uint8_t)(i >> 1);
    uint8_t mask = (i & 1) ? 0x0F : 0xF0;
    uint8_t fill = (i == wpos && wdraw) ? 2 : 1;
    uint8_t y;
    for (y = 0; y < 32; y++) {
        uint8_t c = y < topy ? 0 : (y < topy + 2 ? 3 : ((y & 1) || fill == 2 ? fill : 0));
        uint8_t *p = &dynbuf[(uint16_t)((y >> 3) * 16 + tx) * 16 + ((y & 7) << 1)];
        if (c & 1) p[0] |= mask; else p[0] &= (uint8_t)~mask;
        if (c & 2) p[1] |= mask; else p[1] &= (uint8_t)~mask;
    }
}

static void wave_upload_col(uint8_t i)
{
    uint8_t tx = (uint8_t)(i >> 1), ty;
    for (ty = 0; ty < 4; ty++) {
        uint8_t t = (uint8_t)(ty * 16 + tx);
        dyn_tile(t, &dynbuf[(uint16_t)t * 16]);
    }
}

static void wave_full(void)
{
    uint8_t i, t[16], k;
    for (i = 0; i < 32; i++) wave_render_sample(i);
    set_bkg_data(T_DYN, 64, dynbuf);
    for (k = 0; k < 4; k++) {
        for (i = 0; i < 16; i++) t[i] = (uint8_t)(T_DYN + k * 16 + i);
        put_tiles(2, (uint8_t)(3 + k), 16, t);
    }
}

static void wave_rows(void)
{
    static const char * const rows[5] = { "slot", "draw", "template", "transform", "listen" };
    uint8_t i;
    for (i = 0; i < 5; i++) {
        uint8_t y = (uint8_t)(9 + i);
        uint8_t adj = sub_cur == i && (i == 0 || i == 2 || i == 3);
        put_tile(0, y, sub_cur == i ? T_CURSOR : 0);
        put_low(1, y, rows[i], 9);
        put_tile(10, y, adj ? T_LARROW : 0);
        put_tile(19, y, adj ? T_RARROW : 0);
    }
    lb[0] = (char)('1' + wslot); lb[1] = 0;
    put_strn(11, 9, lb, 8);
    put_low(11, 10, wdraw ? "drawing" : "a: start", 8);
    put_low(11, 11, wt_names[wtempl], 8);
    put_low(11, 12, wx_names[wxform], 8);
    put_low(11, 13, "a: play", 8);
    fmt_num(lb, wpos + 1, 2); lb[2] = 0;
    put_low(1, 8, "sample", 6);
    put_strn(8, 8, lb, 2);
    put_low(11, 8, "level", 5);
    put_num(17, 8, wave_get(wslot, wpos), 2);
}

static void draw_wave(void)
{
    gfx_cls();
    ui_header("WAVE LAB");
    wave_full();
    wave_rows();
    ui_help(wdraw ? "@WAVE_DRAW" : "@WAVE_MENU");
}

static uint8_t find_wave_inst(void)
{
    uint8_t i;
    for (i = 0; i < NUM_INST; i++)
        if (proj.inst[i].p[IP_TYPE] == IT_WAVE && proj.inst[i].p[IP_WSLOT] == wslot) return i;
    for (i = 0; i < NUM_INST; i++) if (proj.inst[i].p[IP_TYPE] == IT_WAVE) return i;
    return 0xFF;
}

static void wave_keys(uint8_t keys)
{
    if (wdraw) {
        uint8_t old = wpos;
        if (keys & K_B) { wdraw = 0; wave_render_sample(wpos); wave_upload_col(wpos); wave_rows(); ui_help("@WAVE_MENU"); return; }
        if (keys & K_LEFT) wpos = (uint8_t)((wpos + 31) & 31);
        if (keys & K_RIGHT) wpos = (uint8_t)((wpos + 1) & 31);
        if (keys & (K_UP | K_DOWN)) {
            uint8_t v = wave_get(wslot, wpos);
            if ((keys & K_UP) && v < 15) v++;
            if ((keys & K_DOWN) && v > 0) v--;
            wave_set(wslot, wpos, v);
        }
        if (keys & K_A) {
            uint8_t wi = find_wave_inst();
            if (wi != 0xFF) engine_audition(wi, 60, 24);
        }
        if (old != wpos) { wave_render_sample(old); wave_upload_col(old); }
        wave_render_sample(wpos); wave_upload_col(wpos);
        wave_rows();
        return;
    }
    if (keys & K_B) { level = LV_TOP; draw_top(); return; }
    if ((keys & K_UP) && sub_cur > 0) sub_cur--;
    if ((keys & K_DOWN) && sub_cur < 4) sub_cur++;
    if (keys & (K_LEFT | K_RIGHT)) {
        int8_t d = (keys & K_RIGHT) ? 1 : -1;
        if (sub_cur == 0) { wslot = (uint8_t)((wslot + 4 + d) & 3); wave_full(); }
        else if (sub_cur == 2) wtempl = (uint8_t)((wtempl + NUM_WTEMPL + d) % NUM_WTEMPL);
        else if (sub_cur == 3) wxform = (uint8_t)((wxform + NUM_WXFORM + d) % NUM_WXFORM);
    }
    if (keys & K_A) {
        uint8_t wi;
        switch (sub_cur) {
        case 1: wdraw = 1; ui_help("@WAVE_DRAW"); wave_render_sample(wpos); wave_upload_col(wpos); break;
        case 2: wave_template(wslot, wtempl); wave_full(); ui_msg("TEMPLATE APPLIED"); break;
        case 3: wave_transform(wslot, wxform, &eng.rng); wave_full(); ui_msg("TRANSFORMED"); break;
        case 4:
            wi = find_wave_inst();
            if (wi == 0xFF) ui_msg("NO WAVE INSTRUMENT");
            else engine_audition(wi, 60, 30);
            break;
        default: break;
        }
    }
    wave_rows();
}

/* ------------------------------------------------------------ mod / out */
static void mod_live(void)
{
    uint8_t i;
    for (i = 0; i < NUM_MODS; i++) {
        int8_t v = mod_source(i);
        lb[0] = (char)('1' + i);
        lb[1] = 0;
        put_strn((uint8_t)(i * 5), 14, lb, 1);
        put_hbar((uint8_t)(i * 5 + 1), 14, 3, (uint8_t)(v + 128), 255);
    }
}

static void draw_mod(void)
{
    gfx_cls();
    ui_header("modulation");
    list_draw(mod_tab, N_MOD, 2, 12, sub_cur, sub_top);
    mod_live();
}

static void draw_out(void)
{
    gfx_cls();
    ui_header("output");
    list_draw(out_tab, N_OUT, 2, N_OUT, sub_cur, 0);
}

void pg_synth(uint8_t keys, uint8_t mode) BANKED
{
    if (mode == M_ENTER) { level = LV_TOP; naming = 0; wdraw = 0; mode = M_DRAW; }
    if (mode == M_DRAW) {
        switch (level) {
        case LV_INST: draw_inst(); break;
        case LV_WAVE: draw_wave(); break;
        case LV_MOD:  draw_mod(); break;
        case LV_OUT:  draw_out(); break;
        default:      draw_top(); break;
        }
        return;
    }
    if (mode == M_TICK) {
        if (level == LV_MOD && (ui_frame_cnt & 7) == 0) mod_live();
        return;
    }

    switch (level) {
    case LV_TOP: {
        uint8_t old = cur;
        if ((keys & K_UP) && cur > 0) cur--;
        if ((keys & K_DOWN) && cur < NUM_INST + 2) cur++;
        if (old != cur) { top_row(old); top_row(cur); top_help(); }
        if (keys & K_B) { ui_home(); return; }
        if ((keys & (K_LEFT | K_RIGHT)) && cur < NUM_INST) { ui_inst = cur; audition(); }
        if (keys & K_A) {
            if (cur < NUM_INST) {
                ui_inst = cur; level = LV_INST; sub_cur = 0; sub_top = 0;
                ui_var[0] = 0;
                draw_inst();
            } else if (cur == NUM_INST) {
                level = LV_WAVE; sub_cur = 1; wdraw = 0;
                wslot = proj.inst[ui_inst].p[IP_TYPE] == IT_WAVE ? (uint8_t)(proj.inst[ui_inst].p[IP_WSLOT] & 3) : 0;
                draw_wave();
            } else if (cur == NUM_INST + 1) { level = LV_MOD; sub_cur = 0; sub_top = 0; ui_mod = 0; draw_mod(); }
            else { level = LV_OUT; sub_cur = 0; draw_out(); }
        }
        break; }
    case LV_INST: {
        uint8_t n, r, oldtype = proj.inst[ui_inst].p[IP_TYPE];
        const PDesc *t = inst_tab(&n);
        if (naming) {
            if (!text_edit(proj.inst[ui_inst].name, INST_NAME_LEN, 11, (uint8_t)(2 + sub_cur - sub_top), keys, &npos)) {
                naming = 0; draw_inst();
            }
            return;
        }
        r = list_run(t, n, 2, 13, &sub_cur, &sub_top, keys);
        if (r == LI_BACK) { level = LV_TOP; draw_top(); return; }
        if (r == LI_ACTION) {
            if (sub_cur == 0) { naming = 1; npos = 0; text_edit(proj.inst[ui_inst].name, INST_NAME_LEN, 11, (uint8_t)(2 + sub_cur - sub_top), 0, &npos); ui_help("@TEXTEDIT"); }
            else if (sub_cur == 2) {
                patch_factory(ui_inst, ui_var[0]);
                ui_msg("PRESET LOADED");
                draw_inst();
                audition();
            } else audition();
        }
        if (r == LI_CHANGED && proj.inst[ui_inst].p[IP_TYPE] != oldtype) {
            sub_top = 0; sub_cur = 1;
            draw_inst();
        }
        break; }
    case LV_WAVE:
        wave_keys(keys);
        break;
    case LV_MOD:
        if (list_run(mod_tab, N_MOD, 2, 12, &sub_cur, &sub_top, keys) == LI_BACK) { level = LV_TOP; draw_top(); }
        break;
    default:
        if (list_run(out_tab, N_OUT, 2, N_OUT, &sub_cur, &top, keys) == LI_BACK) { level = LV_TOP; draw_top(); }
        else audio_set_master(proj.glob[GP_MVOL]);
        break;
    }
}
