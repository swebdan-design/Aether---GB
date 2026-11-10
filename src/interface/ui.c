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
            ui_msg("D-PAD SHAPES MOOD");
            return;
        }
        if ((f & 31) == 0) put_strn(4, 15, (f & 32) ? "            " : "press  start", 12);
        f++;
        engine_service(audio_time());
        wait_vbl_done();
    }
}

/* ================================================================ PLAY
 *
 * The home screen, played like an instrument:
 *
 *  r0   (o) aether                                   play state
 *  r1   ............................
 *  r2   song name
 *  r3   part (scene) name                            elapsed
 *  r4-6 trace: the last ~13 s of notes
 *  r7   tense
 *  r8-13 the MOOD PAD: d-pad moves the dot.
 *        left/right = sparse .. dense, down/up = calm .. tense
 *  r14  calm   <sparse  dense>
 *  r15  ............................
 *  r16  a keep phrase  b next part
 *  r17  sel menu  start play      (or a message)
 *
 * The pad drives all six macros at once; scenes that glide the macros
 * move the dot, so the pad always shows the state of the music.
 */
#define PAD_X0 2
#define PAD_Y0 8
#define PAD_H  6
#define TRACE_ROWS 3
uint8_t dynbuf[64 * 16];                /* 2bpp shadow of 64 dynamic tiles (trace / waveform) */
#define trace dynbuf
static uint8_t trace_x, trace_div;
static uint8_t dirty[8];                /* 64 dirty bits */

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

static uint8_t pad_x, pad_y, pad_drawn_x = 0xFF, pad_drawn_y = 0xFF;

static uint8_t clamp15(int16_t v) { return (uint8_t)(v < 0 ? 0 : v > 15 ? 15 : v); }

/* one gesture -> six macros */
static void pad_apply(void)
{
    uint8_t x = pad_x, t = (uint8_t)(pad_y * 3);
    uint8_t m[NUM_MACROS], k;
    m[MAC_DENSITY]   = x;
    m[MAC_MOTION]    = clamp15(2 + ((x * 3) >> 2));
    m[MAC_SPACE]     = clamp15(13 - ((x * 2) / 3));
    m[MAC_HARMONY]   = t;
    m[MAC_EVOLUTION] = clamp15(3 + ((t * 2) / 3));
    m[MAC_TEXTURE]   = clamp15(3 + (t >> 1));
    for (k = 0; k < NUM_MACROS; k++) { proj.macro[k] = m[k]; eng.form.mac_tgt[k] = m[k]; }
}

static void pad_read(void)
{
    pad_x = proj.macro[MAC_DENSITY] & 15;
    pad_y = (uint8_t)((proj.macro[MAC_HARMONY] + 1) / 3);
    if (pad_y > PAD_H - 1) pad_y = PAD_H - 1;
}

static void pad_cell(uint8_t x, uint8_t y, uint8_t on)
{
    put_tile((uint8_t)(PAD_X0 + x), (uint8_t)(PAD_Y0 + PAD_H - 1 - y), on ? T_DOT_ON : T_DOT_DIM);
}

static void pad_draw_all(void)
{
    uint8_t y, t[16];
    memset(t, T_DOT_DIM, 16);
    for (y = 0; y < PAD_H; y++) put_tiles(PAD_X0, (uint8_t)(PAD_Y0 + y), 16, t);
    pad_drawn_x = pad_drawn_y = 0xFF;
}

static void pad_show(void)
{
    if (pad_drawn_x == pad_x && pad_drawn_y == pad_y) return;
    if (pad_drawn_x != 0xFF) pad_cell(pad_drawn_x, pad_drawn_y, 0);
    pad_cell(pad_x, pad_y, 1);
    pad_drawn_x = pad_x; pad_drawn_y = pad_y;
}

static void play_legend(void)
{
    put_low(0, 16, "a keep   b next part", 20);
    ui_hint(eng.playing ? "sel menu  start stop" : "sel menu  start play");
}

static void play_song_line(void)
{
    put_low(1, 2, proj.name, PROJ_NAME_LEN);
}

static void play_part_line(void)
{
    uint8_t i;
    char s[SCENE_NAME_LEN + 1];
    for (i = 0; i < SCENE_NAME_LEN; i++) s[i] = proj.scene[eng.form.scene & 7].name[i];
    s[SCENE_NAME_LEN] = 0;
    put_low(1, 3, "part", 4);
    put_low(6, 3, s, SCENE_NAME_LEN);
    fmt_time(lb, eng.form.total_s, eng.form.hours);
    if (eng.form.hours) put_strn(13, 3, lb, 7);
    else { put_strn(14, 3, "", 1); put_strn(15, 3, lb, 5); }
}

static void play_draw(void)
{
    gfx_cls();
    ui_header(0);
    play_song_line();
    play_part_line();
    trace_reset();
    put_tile(0, 7, T_UP);
    put_low(1, 7, "tense", 5);
    pad_draw_all();
    put_tile(0, 14, T_DOWN);
    put_low(1, 14, "calm", 4);
    put_tile(6, 14, T_LARROW);
    put_low(7, 14, "sparse", 6);
    put_low(14, 14, "dense", 5);
    put_tile(19, 14, T_RARROW);
    pad_read();
    pad_show();
    play_legend();
}

static uint8_t next_part(void)
{
    uint8_t k, s = eng.form.scene;
    for (k = 1; k <= NUM_SCENES; k++) {
        uint8_t n = (uint8_t)((s + k) & 7);
        if (proj.scene[n].s[SC_ON]) return n;
    }
    return s;
}

void pg_live(uint8_t keys, uint8_t mode) BANKED
{
    static uint8_t was_playing;
    if (mode == M_ENTER) { ui_panel_single = 1; mode = M_DRAW; }
    if (mode == M_DRAW) { play_draw(); was_playing = eng.playing; return; }
    if (mode == M_TICK) {
        uint8_t ph = ui_frame_cnt & 7;
        trace_tick();
        if (ph == 3) { pad_read(); pad_show(); }          /* scenes move the dot */
        else if (ph == 5 && (ui_frame_cnt & 31) == 5) play_part_line();
        if (was_playing != eng.playing) { was_playing = eng.playing; if (!ui_msg_t) play_legend(); }
        return;
    }
    if (keys & (K_LEFT | K_RIGHT | K_UP | K_DOWN)) {
        pad_read();
        if ((keys & K_LEFT) && pad_x > 0) pad_x--;
        if ((keys & K_RIGHT) && pad_x < 15) pad_x++;
        if ((keys & K_DOWN) && pad_y > 0) pad_y--;
        if ((keys & K_UP) && pad_y < PAD_H - 1) pad_y++;
        pad_apply();
        pad_show();
    }
    if (keys & K_A) ui_capture_imprint(0xFF);
    if (keys & K_B) {
        uint8_t n = next_part();
        if (n == eng.form.scene) ui_msg("ONLY ONE PART");
        else {
            uint8_t i;
            form_goto(n, 0);
            strcpy(lb, "NEXT: ");
            for (i = 0; i < SCENE_NAME_LEN; i++) lb[6 + i] = proj.scene[n].name[i];
            lb[6 + SCENE_NAME_LEN] = 0;
            ui_msg(lb);
            play_part_line();
        }
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
    case PG_PROJECT: pg_project(keys, mode); break;
    case PG_MENU:    pg_menu(keys, mode); break;
    case PG_SONGS:   pg_songs(keys, mode); break;
    default:         pg_tune(keys, mode); break;
    }
}

void ui_redraw(void) BANKED
{
    ui_panel_single = 0;
    ui_msg_t = 0;
    page_call(0, M_ENTER);
    ui_panel_draw();
}

void ui_goto(uint8_t page) BANKED
{
    ui_page = page;
    ui_redraw();
}

/* B on the top level of an editor returns to the menu */
void ui_home(void) BANKED
{
    ui_goto(PG_MENU);
}

void ui_frame(void) BANKED
{
    uint8_t keys = joy_poll();
    static uint8_t was_playing;
    ui_frame_cnt++;
    if (keys & K_START) { ui_toggle_play(); keys &= (uint8_t)~K_START; }
    if (keys & K_SELECT) {
        /* SELECT: the menu; from the menu, back to playing */
        ui_goto(ui_page == PG_MENU ? PG_LIVE : PG_MENU);
        return;
    }
    if (keys) page_call(keys, M_KEYS);
    page_call(0, M_TICK);
    if (was_playing != eng.playing) { was_playing = eng.playing; put_tile(19, 0, eng.playing ? T_PLAY : T_STOP); }
    if (ui_msg_t && --ui_msg_t == 0) ui_panel_draw();
}
