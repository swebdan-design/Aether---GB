/*
 * AETHER - by Synth Centric
 * ui.c : page framework, title screen and the LIVE performance page.
 *
 * SELECT cycles pages, START toggles playback anywhere, the bottom line
 * carries contextual hints and short messages.  The LIVE page shows the
 * evolving system: a sweeping pitch/time trace, agent activity, channel
 * meters, the four ecology variables, tide / erosion / chord, the six
 * macro controls and the scene selector.  A captures an IMPRINT.
 */
#pragma bank 255
#include <gb/gb.h>
#include <string.h>
#include "ui.h"
#include "engine.h"
#include "audio.h"

uint8_t ui_page;
uint8_t ui_frame_cnt;
uint8_t imprint_next;
uint8_t ui_panel_single;            /* LIVE: help panel uses row 17 only */

static const char * const scale_short[NUM_SCALES] = {
    "major", "minor", "maj pent", "min pent", "dorian", "phrygian", "lydian", "mixolyd", "chromatic", "user set"
};
static const char note_names[] = "C C#D D#E F F#G G#A A#B ";
static const char * const macro_names[NUM_MACROS] = {
    "density", "evolution", "harmony", "motion", "texture", "space"
};
static const char * const roman[7] = { "I", "II", "III", "IV", "V", "VI", "VII" };
static const char * const agent_short[NUM_AGENTS] = {
    "orb", "blm", "drf", "sct", "tid", "ech", "ero", "mem"
};

void ui_toggle_play(void) BANKED
{
    engine_set_playing((uint8_t)!eng.playing);
    put_tile(19, 0, eng.playing ? T_PLAY : T_STOP);
    ui_msg(eng.playing ? "PLAY" : "STOP");
}

uint8_t ui_capture_imprint(uint8_t agent_filter) BANKED
{
    uint8_t n = motif_capture(imprint_next, agent_filter, 6);
    if (!n) { ui_msg("NOTHING TO IMPRINT"); return 0; }
    strcpy(lb, "IMPRINT ");
    lb[8] = (char)('1' + imprint_next);
    strcpy(lb + 9, " CAPTURED ");
    lb[19] = (char)('0' + n);
    lb[20] = 0;
    ui_msg(lb);
    imprint_next = (uint8_t)((imprint_next + 1) % NUM_MOTIFS);
    return n;
}

void ui_scene_label(uint8_t s, char *out) BANKED
{
    uint8_t i;
    out[0] = 'S'; out[1] = (char)('1' + (s & 7)); out[2] = ' ';
    for (i = 0; i < SCENE_NAME_LEN; i++) out[3 + i] = proj.scene[s & 7].name[i];
    out[11] = 0;
}

/* -------------------------------------------------------------- title */
void ui_title(void) BANKED
{
    uint8_t i, t[16], f = 0, k;
    gfx_cls();
    gfx_title_tiles();
    for (k = 0; k < 3; k++) {
        for (i = 0; i < 16; i++) t[i] = (uint8_t)(T_DYN + k * 16 + i);
        put_tiles(2, (uint8_t)(3 + k), 16, t);
    }
    put_str(2, 7, "by Synth Centric");
    for (i = 0; i < 16; i++) t[i] = (uint8_t)(T_DYN + 48 + i);
    put_tiles(2, 9, 16, t);
    put_str(1, 11, "generative ambient");
    put_str(5, 12, "instrument");
    put_str(14, 17, "v" AETHER_VERSION_STR);
    while (1) {
        uint8_t keys = joy_poll();
        if (keys & (K_START | K_A)) {
            ui_page = PG_LIVE;
            ui_redraw();
            if (keys & K_START) ui_toggle_play();
            return;
        }
        if ((f & 31) == 0) put_strn(4, 15, (f & 32) ? "            " : "press  start", 12);
        f++;
        engine_service(audio_time());
        wait_vbl_done();
    }
}

/* ================================================================ LIVE
 *
 *  r0   (o) live > project name                        play
 *  r1   ............................ page dots
 *  r2   key + scale          free / bpm         elapsed
 *  r3   scene name                              chord
 *  r4-6 trace: the last ~13 s of generated pitches
 *  r7-8 the eight agents: activity dot + name
 *  r9-10 ecology meters
 *  r11  ............................
 *  r12-13 six macro faders + scene slot
 *  r14  selection marker
 *  r15  ............................
 *  r16  readout of the selected control
 *  r17  help / messages
 */
#define LIVE_SCENE 6
#define TRACE_ROWS 3
static uint8_t live_cur;
static uint8_t live_scene_sel;
uint8_t dynbuf[64 * 16];                /* 2bpp shadow of 64 dynamic tiles (trace / waveform) */
#define trace dynbuf
static uint8_t trace_x, trace_div;
static uint8_t dirty[8];                /* 64 dirty bits */
static uint8_t mac_shown[NUM_MACROS];

static void trace_px(uint8_t x, uint8_t y, uint8_t c)
{
    uint8_t tile = (uint8_t)((y >> 3) * 16 + (x >> 3));
    uint8_t *p = &trace[(uint16_t)tile * 16 + ((y & 7) << 1)];
    uint8_t bit = (uint8_t)(0x80 >> (x & 7));
    if (c & 1) p[0] |= bit; else p[0] &= (uint8_t)~bit;
    if (c & 2) p[1] |= bit; else p[1] &= (uint8_t)~bit;
    dirty[tile >> 3] |= (uint8_t)(1 << (tile & 7));
}

static void trace_col_clear(uint8_t x, uint8_t c)
{
    /* clear one pixel column with direct byte ops; c = 1: dotted sweep line */
    uint8_t ty, r;
    uint8_t bit = (uint8_t)(0x80 >> (x & 7));
    uint8_t nbit = (uint8_t)~bit;
    uint8_t tile = (uint8_t)(x >> 3);
    for (ty = 0; ty < TRACE_ROWS; ty++, tile += 16) {
        uint8_t *p = &trace[(uint16_t)tile << 4];
        for (r = 0; r < 8; r++, p += 2) {
            p[1] &= nbit;
            if (c && !(r & 1)) p[0] |= bit; else p[0] &= nbit;
        }
        dirty[tile >> 3] |= (uint8_t)(1 << (tile & 7));
    }
}

static void trace_upload(uint8_t maxn)
{
    uint8_t i, b;
    for (i = 0; i < 6 && maxn; i++) {
        if (!dirty[i]) continue;
        for (b = 0; b < 8 && maxn; b++) {
            if (dirty[i] & (1 << b)) {
                uint8_t tile = (uint8_t)(i * 8 + b);
                dyn_tile(tile, &trace[(uint16_t)tile * 16]);
                dirty[i] &= (uint8_t)~(1 << b);
                maxn--;
            }
        }
    }
}

static void trace_reset(void)
{
    uint8_t i, t[16], k;
    memset(trace, 0, sizeof(trace));
    memset(dirty, 0xFF, sizeof(dirty));
    for (k = 0; k < TRACE_ROWS; k++) {
        for (i = 0; i < 16; i++) t[i] = (uint8_t)(T_DYN + k * 16 + i);
        put_tiles(2, (uint8_t)(4 + k), 16, t);
    }
    put_tile(0, 4, T_NOTE);
}

static void trace_tick(void)
{
    while (eng.viz_r != eng.viz_w) {
        VizEv *v = &eng.viz[eng.viz_r];
        uint8_t y;
        if (v->ch == CH_NOISE) {
            y = (uint8_t)(18 + (v->note & 5));
            trace_px(trace_x, y, 1);
        } else {
            uint8_t n = v->note < 36 ? 36 : (v->note > 99 ? 99 : v->note);
            y = (uint8_t)(23 - (((n - 36) * 3) >> 3));
            trace_px(trace_x, y, 3);
            if (trace_x < 127) trace_px((uint8_t)(trace_x + 1), y, v->ch == CH_WAVE ? 2 : 3);
        }
        eng.viz_r = (eng.viz_r + 1) & (VIZ_N - 1);
    }
    if (++trace_div >= 6) {
        trace_div = 0;
        if (eng.playing) {
            trace_x = (uint8_t)((trace_x + 1) & 127);
            trace_col_clear(trace_x, 0);
            trace_col_clear((uint8_t)((trace_x + 1) & 127), 1);
        }
    }
    trace_upload(2);
}

/* ---- faders: two tiles wide, two tiles tall, 16 levels */
static void live_fader(uint8_t k)
{
    uint8_t x = (uint8_t)(1 + k * 3);
    uint8_t h = (uint8_t)(proj.macro[k] + 1);              /* 1..16 px */
    uint8_t lo = h > 8 ? 8 : h, hi = h > 8 ? (uint8_t)(h - 8) : 0;
    uint8_t t[2];
    t[0] = t[1] = (uint8_t)(T_VBAR + hi);
    put_tiles(x, 12, 2, t);
    t[0] = t[1] = (uint8_t)(T_VBAR + lo);
    put_tiles(x, 13, 2, t);
    mac_shown[k] = proj.macro[k];
}

static void live_markers(void)
{
    uint8_t t[20], k;
    memset(t, 0, 20);
    for (k = 0; k < NUM_MACROS; k++) t[1 + k * 3] = t[2 + k * 3] = T_DOTSM;
    if (live_cur < NUM_MACROS) { t[1 + live_cur * 3] = T_MARK; t[2 + live_cur * 3] = T_MARK; }
    else t[19] = T_MARK;
    put_tiles(0, 14, 20, t);
}

static void live_scene_slot(void)
{
    /* the seventh control: scene number as a two-line glyph */
    lb[0] = 's'; lb[1] = 0;
    put_strn(19, 12, lb, 1);
    lb[0] = (char)('1' + live_scene_sel);
    put_strn(19, 13, lb, 1);
}

static void live_readout(void)
{
    char s[12];
    put_strn(0, 16, "", 20);
    if (live_cur < NUM_MACROS) {
        put_low(1, 16, macro_names[live_cur], 10);
        put_tile(15, 16, T_DOWN);
        put_num(16, 16, proj.macro[live_cur], 2);
        put_tile(18, 16, T_UP);
    } else {
        put_low(1, 16, "scene", 5);
        put_tile(7, 16, T_DOWN);
        ui_scene_label(live_scene_sel, s);
        put_low(8, 16, s, 11);
        put_tile(19, 16, T_UP);
        if (live_scene_sel == eng.form.scene) put_tile(0, 16, T_DOTSM);
    }
}

static void live_help(void)
{
    ui_hint(live_cur < NUM_MACROS ? "u/d value  a imprint" : "u/d pick  a go there");
}

static void live_key_line(void)
{
    uint8_t r = proj.harm[HP_ROOT] % 12;
    char s[21];
    memset(s, ' ', 20);
    s[20] = 0;
    s[0] = note_names[r * 2];
    s[1] = note_names[r * 2 + 1];
    { uint8_t i; const char *n = scale_short[proj.harm[HP_SCALE] % NUM_SCALES]; for (i = 0; i < 9 && n[i]; i++) s[3 + i] = n[i]; }
    if (proj.glob[GP_TMODE] == TM_FREE) { s[16] = 'f'; s[17] = 'r'; s[18] = 'e'; s[19] = 'e'; }
    else { fmt_num(s + 13, proj.glob[GP_TEMPO], 3); s[16] = 'b'; s[17] = 'p'; s[18] = 'm'; }
    put_strn(0, 2, s, 20);
}

static void live_scene_line(void)
{
    char s[12];
    ui_scene_label(eng.form.scene, s);
    put_low(0, 3, s, 12);
    fmt_time(lb, eng.form.total_s, eng.form.hours);
    if (eng.form.hours) put_strn(13, 3, lb, 7);
    else put_strn(15, 3, lb, 5);
}

static void live_agents(void)
{
    uint8_t i, t[20];
    for (i = 0; i < NUM_AGENTS; i++) {
        uint8_t on = proj.agent[i].a[AP_EN] && ((eng.form.live.s[SC_MASK] >> i) & 1);
        uint8_t x = (uint8_t)((i & 3) * 5);
        const char *n = agent_short[i];
        t[x] = !on ? 0 : ((eng.ag[i].fired && eng.playing) ? T_DOT_ON : T_DOTSM);
        t[x + 1] = (uint8_t)(n[0] - 32);
        t[x + 2] = (uint8_t)(n[1] - 32);
        t[x + 3] = (uint8_t)(n[2] - 32);
        t[x + 4] = 0;
        if ((i & 3) == 3) put_tiles(0, (uint8_t)(7 + (i >> 2)), 20, t);
    }
}

static void live_ecology(void)
{
    put_low(0, 9, "energy", 6);
    put_hbar(6, 9, 3, ECO8(ECO_ENERGY), 255);
    put_low(10, 9, "tension", 7);
    put_hbar(17, 9, 3, ECO8(ECO_TENSION), 255);
    put_low(0, 10, "stable", 6);
    put_hbar(6, 10, 3, ECO8(ECO_STAB), 255);
    put_low(10, 10, "memory", 7);
    put_hbar(17, 10, 3, ECO8(ECO_MEMORY), 255);
}

static void live_draw(void)
{
    uint8_t k;
    char nm[PROJ_NAME_LEN + 1];
    memcpy(nm, proj.name, PROJ_NAME_LEN);
    nm[PROJ_NAME_LEN] = 0;
    for (k = PROJ_NAME_LEN; k > 0 && nm[k - 1] == ' '; k--) nm[k - 1] = 0;
    gfx_cls();
    ui_header(nm);
    put_strn(0, 15, "", 20);
    put_rule(11);
    put_rule(15);
    live_key_line();
    live_scene_line();
    trace_reset();
    live_agents();
    live_ecology();
    for (k = 0; k < NUM_MACROS; k++) live_fader(k);
    live_scene_slot();
    live_markers();
    live_readout();
    live_help();
}

static void live_select(uint8_t c)
{
    live_cur = c;
    live_markers();
    live_readout();
    live_help();
}

void pg_live(uint8_t keys, uint8_t mode) BANKED
{
    if (mode == M_ENTER) { live_scene_sel = eng.form.scene; ui_panel_single = 1; mode = M_DRAW; }
    if (mode == M_DRAW) { live_draw(); return; }
    if (mode == M_TICK) {
        uint8_t ph = ui_frame_cnt & 7, k;
        trace_tick();
        if (ph == 1) live_agents();
        else if (ph == 3) live_ecology();
        else if (ph == 5) {
            for (k = 0; k < NUM_MACROS; k++)
                if (mac_shown[k] != proj.macro[k]) {
                    live_fader(k);
                    if (k == live_cur) live_readout();
                }
        } else if (ph == 7) {
            if ((ui_frame_cnt & 63) == 7) live_key_line();
            else if ((ui_frame_cnt & 31) == 15) live_scene_line();
        }
        return;
    }
    if ((keys & K_LEFT) && live_cur > 0) live_select((uint8_t)(live_cur - 1));
    if ((keys & K_RIGHT) && live_cur < LIVE_SCENE) live_select((uint8_t)(live_cur + 1));
    if (keys & (K_UP | K_DOWN)) {
        if (live_cur < NUM_MACROS) {
            uint8_t *m = &proj.macro[live_cur];
            if ((keys & K_UP) && *m < 15) (*m)++;
            if ((keys & K_DOWN) && *m > 0) (*m)--;
            eng.form.mac_tgt[live_cur] = *m;      /* the performer overrides any glide */
            live_fader(live_cur);
        } else {
            uint8_t k, s = live_scene_sel;
            for (k = 0; k < NUM_SCENES; k++) {
                s = (uint8_t)((keys & K_UP) ? (s + 1) & 7 : (s + 7) & 7);
                if (proj.scene[s].s[SC_ON]) break;
            }
            live_scene_sel = s;
            live_scene_slot();
        }
        live_readout();
    }
    if (keys & K_A) {
        if (live_cur == LIVE_SCENE) {
            form_goto(live_scene_sel, 0);
            ui_msg("SCENE CHANGE");
            live_readout();
            live_scene_line();
        } else ui_capture_imprint(0xFF);
    }
}

/* ============================================================ framework */
void ui_init(void) BANKED
{
    ui_page = PG_LIVE;
    ui_frame_cnt = 0;
    imprint_next = 0;
    ui_hint("");
}

static void page_call(uint8_t keys, uint8_t mode)
{
    switch (ui_page) {
    case PG_LIVE:    pg_live(keys, mode); break;
    case PG_SYNTH:   pg_synth(keys, mode); break;
    case PG_AGENTS:  pg_agents(keys, mode); break;
    case PG_HARMONY: pg_harmony(keys, mode); break;
    case PG_FORM:    pg_form(keys, mode); break;
    default:         pg_project(keys, mode); break;
    }
}

void ui_redraw(void) BANKED
{
    ui_panel_single = 0;
    ui_msg_t = 0;
    page_call(0, M_ENTER);
    ui_panel_draw();
}

/* B on the top level of any page returns home to LIVE */
void ui_home(void) BANKED
{
    ui_page = PG_LIVE;
    ui_redraw();
}

void ui_frame(void) BANKED
{
    uint8_t keys = joy_poll();
    static uint8_t was_playing;
    ui_frame_cnt++;
    if (keys & K_START) { ui_toggle_play(); keys &= (uint8_t)~K_START; }
    if (keys & K_SELECT) {
        ui_page = (uint8_t)((ui_page + 1) % NUM_PAGES);
        ui_redraw();
        return;
    }
    if (keys) page_call(keys, M_KEYS);
    page_call(0, M_TICK);
    if (was_playing != eng.playing) { was_playing = eng.playing; put_tile(19, 0, eng.playing ? T_PLAY : T_STOP); }
    if (ui_msg_t && --ui_msg_t == 0) ui_panel_draw();
}
