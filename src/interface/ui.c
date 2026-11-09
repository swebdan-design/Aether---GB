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
extern char ui_msgbuf[21];
extern char ui_hintbuf[21];

static const char * const page_names[NUM_PAGES] = {
    "LIVE", "SYNTH", "AGENTS", "HARMONY", "FORM", "PROJECT"
};
static const char * const scale_short[NUM_SCALES] = {
    "MAJOR", "MINOR", "MAJ PENT", "MIN PENT", "DORIAN", "PHRYGIAN", "LYDIAN", "MIXOLYD", "CHROMAT", "USER"
};
static const char note_names[] = "C C#D D#E F F#G G#A A#B ";
static const char * const macro_names[NUM_MACROS] = {
    "DENSITY", "EVOLUTION", "HARMONY", "MOTION", "TEXTURE", "SPACE"
};
static const char * const roman[7] = { "I  ", "II ", "III", "IV ", "V  ", "VI ", "VII" };

/* ------------------------------------------------------------ header */
void ui_header(const char *sub) BANKED
{
    uint8_t i;
    char t[12];
    memset(t, ' ', 11);
    t[11] = 0;
    for (i = 0; i < 10 && page_names[ui_page][i]; i++) t[1 + i] = page_names[ui_page][i];
    put_inv(0, 0, t, 11);
    if (sub) put_strn(11, 0, sub, 9);
    else {
        put_tile(11, 0, 0);
        for (i = 0; i < NUM_PAGES; i++) put_tile((uint8_t)(12 + i), 0, i == ui_page ? T_PAGE_ON : T_PAGE_OFF);
        put_tile(18, 0, 0);
    }
    put_tile(19, 0, eng.playing ? T_PLAY : T_STOP);
}

static uint8_t last_msg_shown = 1;
static void draw_hint(void)
{
    if (ui_msg_t) { put_inv(0, 17, ui_msgbuf, 20); last_msg_shown = 1; }
    else put_strn(0, 17, ui_hintbuf, 20);
}

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
    put_str(13, 17, "v" AETHER_VERSION_STR);
    while (1) {
        uint8_t keys = joy_poll();
        if (keys & (K_START | K_A)) {
            ui_page = PG_LIVE;
            ui_redraw();
            if (keys & K_START) ui_toggle_play();
            return;
        }
        if ((f & 31) == 0) put_strn(4, 15, (f & 32) ? "            " : "PRESS  START", 12);
        f++;
        engine_service(audio_time());
        wait_vbl_done();
    }
}

/* ================================================================ LIVE */
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
    uint8_t y;
    for (y = 0; y < 32; y++) trace_px(x, y, (y & 1) == 0 ? c : 0);
}

static void trace_upload(uint8_t maxn)
{
    uint8_t i, b;
    for (i = 0; i < 8 && maxn; i++) {
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
    for (k = 0; k < 4; k++) {
        for (i = 0; i < 16; i++) t[i] = (uint8_t)(T_DYN + k * 16 + i);
        put_tiles(2, (uint8_t)(3 + k), 16, t);
    }
    put_tile(1, 3, T_NOTE);
}

static void trace_tick(void)
{
    /* plot newly generated events */
    while (eng.viz_r != eng.viz_w) {
        VizEv *v = &eng.viz[eng.viz_r];
        uint8_t y;
        if (v->ch == CH_NOISE) {
            y = (uint8_t)(24 + (v->note & 7));
            trace_px(trace_x, y, 1);
        } else {
            uint8_t n = v->note < 36 ? 36 : (v->note > 99 ? 99 : v->note);
            y = (uint8_t)(31 - ((n - 36) >> 1));
            trace_px(trace_x, y, 3);
            if (trace_x < 127) trace_px((uint8_t)(trace_x + 1), y, v->ch == CH_WAVE ? 2 : 3);
        }
        eng.viz_r = (eng.viz_r + 1) & (VIZ_N - 1);
    }
    if (++trace_div >= 6) {
        trace_div = 0;
        if (eng.playing) {
            trace_x = (uint8_t)((trace_x + 1) & 127);
            trace_col_clear(trace_x, 0);                          /* old cursor column */
            trace_col_clear((uint8_t)((trace_x + 1) & 127), 1);  /* new cursor + wipe */
        }
    }
    trace_upload(2);
}

static void live_row_macro(uint8_t k)
{
    uint8_t y = (uint8_t)(10 + k);
    uint8_t sel = live_cur == k;
    put_tile(0, y, sel ? T_CURSOR : 0);
    if (sel) put_inv(1, y, macro_names[k], 9); else put_strn(1, y, macro_names[k], 9);
    put_tile(10, y, 0);
    put_hbar(11, y, 6, proj.macro[k], 15);
    put_tile(17, y, 0);
    put_num(18, y, proj.macro[k], 2);
    mac_shown[k] = proj.macro[k];
}

static void live_row_scene(void)
{
    char s[12];
    uint8_t sel = live_cur == 6;
    put_tile(0, 16, sel ? T_CURSOR : 0);
    if (sel) put_inv(1, 16, "SCENE", 5); else put_strn(1, 16, "SCENE", 5);
    put_tile(6, 16, T_LARROW);
    ui_scene_label(live_scene_sel, s);
    put_strn(7, 16, s, 11);
    put_tile(18, 16, T_RARROW);
    put_tile(19, 16, live_scene_sel == eng.form.scene ? T_DOT_ON : T_DOT_OFF);
}

static void live_key_line(void)
{
    uint8_t r = proj.harm[HP_ROOT] % 12;
    char s[21];
    memset(s, ' ', 20);
    s[20] = 0;
    s[0] = note_names[r * 2];
    s[1] = note_names[r * 2 + 1];
    { uint8_t i; const char *n = scale_short[proj.harm[HP_SCALE] % NUM_SCALES]; for (i = 0; i < 8 && n[i]; i++) s[3 + i] = n[i]; }
    if (proj.glob[GP_TMODE] == TM_FREE) { s[12] = 'F'; s[13] = 'R'; s[14] = 'E'; s[15] = 'E'; }
    else { fmt_num(s + 12, proj.glob[GP_TEMPO], 3); s[15] = 'B'; }
    put_strn(0, 2, s, 16);
    fmt_time(lb, eng.form.total_s, eng.form.hours);
    if (eng.form.hours) put_strn(13, 2, lb, 7);
    else { put_strn(16, 2, lb + 1, 4); put_tile(15, 2, 0); }
}

static void live_agents_line(void)
{
    static const char letters[] = "OBDSTERM";
    uint8_t i, t[20];
    for (i = 0; i < NUM_AGENTS; i++) {
        uint8_t on = proj.agent[i].a[AP_EN] && ((eng.form.live.s[SC_MASK] >> i) & 1);
        uint8_t c = (uint8_t)letters[i];
        if (!on) t[i * 2] = (uint8_t)('.' - 32);
        else if (eng.ag[i].fired && eng.playing) t[i * 2] = (uint8_t)(T_INV + c - 32);
        else t[i * 2] = (uint8_t)(c - 32);
        t[i * 2 + 1] = 0;
    }
    for (i = 0; i < NUM_CH; i++) t[16 + i] = (uint8_t)(T_VBAR + ((ch_meter[i] + 1) >> 1 > 8 ? 8 : (ch_meter[i] + 1) >> 1));
    put_tiles(0, 7, 20, t);
}

static void live_eco_line(void)
{
    static const char el[] = "ETSM";
    uint8_t i;
    for (i = 0; i < 4; i++) {
        lb[0] = el[i]; lb[1] = 0;
        put_strn((uint8_t)(i * 5), 8, lb, 1);
        put_hbar((uint8_t)(i * 5 + 1), 8, 3, ECO8(i), 255);
        put_tile((uint8_t)(i * 5 + 4), 8, 0);
    }
}

static void live_state_line(void)
{
    put_strn(0, 9, "~", 1);
    put_hbar(1, 9, 3, eng.tide, 255);
    put_strn(5, 9, "x", 1);
    put_hbar(6, 9, 3, eng.erosion, 255);
    put_strn(10, 9, roman[eng.hs.degree % 7], 3);
    put_strn(14, 9, eng.eco.resolving ? "RESOLV" : "      ", 6);
}

static void live_draw(void)
{
    uint8_t k;
    gfx_cls();
    ui_header(0);
    put_strn(0, 1, proj.name, PROJ_NAME_LEN);
    live_key_line();
    trace_reset();
    live_agents_line();
    live_eco_line();
    live_state_line();
    for (k = 0; k < NUM_MACROS; k++) live_row_macro(k);
    live_row_scene();
    ui_hint("A:IMPRINT START:PLAY");
}

void pg_live(uint8_t keys, uint8_t mode) BANKED
{
    if (mode == M_ENTER) { live_scene_sel = eng.form.scene; mode = M_DRAW; }
    if (mode == M_DRAW) { live_draw(); return; }
    if (mode == M_TICK) {
        uint8_t ph = ui_frame_cnt & 7, k;
        trace_tick();
        if (ph == 1) live_agents_line();
        else if (ph == 3) live_eco_line();
        else if (ph == 5) {
            live_state_line();
            for (k = 0; k < NUM_MACROS; k++) if (mac_shown[k] != proj.macro[k]) live_row_macro(k);
        } else if (ph == 7 && (ui_frame_cnt & 31) == 7) {
            live_key_line();
            if (live_cur != 6 || (joy_held & 0x0F) == 0) live_row_scene();
        }
        return;
    }
    if (keys & K_UP && live_cur > 0) {
        live_cur--;
        if (live_cur == 5) live_row_scene();
        live_row_macro(live_cur);
        if (live_cur + 1 < NUM_MACROS) live_row_macro((uint8_t)(live_cur + 1));
    }
    if (keys & K_DOWN && live_cur < 6) {
        live_cur++;
        live_row_macro((uint8_t)(live_cur - 1));
        if (live_cur < NUM_MACROS) live_row_macro(live_cur); else live_row_scene();
    }
    if (keys & (K_LEFT | K_RIGHT)) {
        if (live_cur < NUM_MACROS) {
            uint8_t *m = &proj.macro[live_cur];
            if ((keys & K_RIGHT) && *m < 15) (*m)++;
            if ((keys & K_LEFT) && *m > 0) (*m)--;
            eng.form.mac_tgt[live_cur] = *m;      /* user overrides any glide */
            live_row_macro(live_cur);
        } else {
            uint8_t k, s = live_scene_sel;
            for (k = 0; k < NUM_SCENES; k++) {
                s = (uint8_t)((keys & K_RIGHT) ? (s + 1) & 7 : (s + 7) & 7);
                if (proj.scene[s].s[SC_ON]) break;
            }
            live_scene_sel = s;
            live_row_scene();
        }
    }
    if (keys & K_A) {
        if (live_cur == 6) {
            form_goto(live_scene_sel, 0);
            ui_msg("SCENE CHANGE");
            live_row_scene();
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
    page_call(0, M_ENTER);
    draw_hint();
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
    if (ui_msg_t) {
        ui_msg_t--;
        if (ui_msg_t == 99 || ui_msg_t == 0) draw_hint();
    } else if ((ui_frame_cnt & 15) == 0) draw_hint();
}
