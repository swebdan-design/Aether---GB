/*
 * AETHER - by Synth Centric
 * gfx.c : display primitives, input with debounce / auto-repeat, and the
 * table-driven parameter list editor.  Fixed bank 0 so that every banked
 * page can pass pointers to its own constant tables and strings.
 */
#include <gb/gb.h>
#include <string.h>
#include "gfx.h"
#include "engine.h"
#include "ui.h"

BANKREF_EXTERN(gfx_data)
extern const uint8_t font_1bpp[];
extern const uint8_t gfx_2bpp[];
extern const uint8_t logo_2bpp[];
extern const uint8_t extra_2bpp[];
void help_lookup(const char *key, char *out) BANKED;
extern const uint8_t wave_2bpp[];

char lb[24];
uint8_t joy_held, joy_accel;
uint8_t ui_agent, ui_inst, ui_scene, ui_mod, ui_motif, ui_sub;
uint8_t ui_var[8];
uint8_t ui_msg_t;
char ui_msgbuf[21];
char ui_help_txt[42];               /* two 20-char lines (+NUL each) */

static uint8_t tb[20];
static uint8_t joy_prev, rep_cnt;

/* ---------------------------------------------------------------- init */
void gfx_init(void) NONBANKED
{
    uint8_t save = CURRENT_BANK;
    DISPLAY_OFF;
    SWITCH_ROM(BANK(gfx_data));
    set_1bpp_colors(DMG_BLACK, DMG_WHITE);
    set_bkg_1bpp_data(0, 95, font_1bpp);
    set_1bpp_colors(DMG_WHITE, DMG_BLACK);
    set_bkg_1bpp_data(T_INV, 59, font_1bpp);
    set_bkg_data(T_UP, 6, extra_2bpp);
    set_bkg_data(T_HBAR, 32, gfx_2bpp);
    SWITCH_ROM(save);
    BGP_REG = 0xE4;
    SCX_REG = 0; SCY_REG = 0;
    gfx_cls();
    SHOW_BKG;
    DISPLAY_ON;
}

void gfx_title_tiles(void) NONBANKED
{
    uint8_t save = CURRENT_BANK;
    SWITCH_ROM(BANK(gfx_data));
    set_bkg_data(T_DYN, 48, logo_2bpp);
    set_bkg_data(T_DYN + 48, 16, wave_2bpp);
    SWITCH_ROM(save);
}

void dyn_tile(uint8_t idx, const uint8_t *data) NONBANKED
{
    set_bkg_data((uint8_t)(T_DYN + idx), 1, data);
}

void gfx_cls(void) NONBANKED
{
    gfx_clear_rows(0, 17);
}

void gfx_clear_rows(uint8_t y0, uint8_t y1) NONBANKED
{
    uint8_t y;
    memset(tb, 0, 20);
    for (y = y0; y <= y1; y++) set_bkg_tiles(0, y, 20, 1, tb);
}

/* ---------------------------------------------------------------- text */
void put_tile(uint8_t x, uint8_t y, uint8_t t) NONBANKED
{
    set_bkg_tile_xy(x, y, t);
}

void put_tiles(uint8_t x, uint8_t y, uint8_t n, const uint8_t *t) NONBANKED
{
    set_bkg_tiles(x, y, n, 1, t);
}

static uint8_t ch_tile(char c)
{
    uint8_t u = (uint8_t)c;
    if (u < 32 || u > 126) return 0;
    return (uint8_t)(u - 32);
}

void put_strn(uint8_t x, uint8_t y, const char *s, uint8_t n) NONBANKED
{
    uint8_t i = 0;
    if (n > 20) n = 20;
    for (; i < n && s[i]; i++) tb[i] = ch_tile(s[i]);
    for (; i < n; i++) tb[i] = 0;
    set_bkg_tiles(x, y, n, 1, tb);
}

void put_str(uint8_t x, uint8_t y, const char *s) NONBANKED
{
    put_strn(x, y, s, (uint8_t)strlen(s));
}

/* the AETHER voice: interface text is set in lowercase */
void put_low(uint8_t x, uint8_t y, const char *s, uint8_t n) NONBANKED
{
    uint8_t i = 0;
    if (n > 20) n = 20;
    for (; i < n && s[i]; i++) {
        uint8_t c = (uint8_t)s[i];
        if (c >= 'A' && c <= 'Z') c += 32;
        tb[i] = ch_tile((char)c);
    }
    for (; i < n; i++) tb[i] = 0;
    set_bkg_tiles(x, y, n, 1, tb);
}

void put_rule(uint8_t y) NONBANKED
{
    memset(tb, T_RULE, 20);
    set_bkg_tiles(0, y, 20, 1, tb);
}

void put_inv(uint8_t x, uint8_t y, const char *s, uint8_t n) NONBANKED
{
    uint8_t i = 0;
    if (n > 20) n = 20;
    for (; i < n && s[i]; i++) {
        uint8_t c = (uint8_t)s[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        if (c < 32 || c > 90) c = 32;
        tb[i] = (uint8_t)(T_INV + c - 32);
    }
    for (; i < n; i++) tb[i] = T_INV;
    set_bkg_tiles(x, y, n, 1, tb);
}

void fmt_num(char *d, uint16_t v, uint8_t w) NONBANKED
{
    uint8_t i;
    for (i = 0; i < w; i++) d[i] = ' ';
    d[w] = 0;
    i = w;
    do {
        d[--i] = (char)('0' + v % 10);
        v /= 10;
    } while (v && i);
}

void put_num(uint8_t x, uint8_t y, uint16_t v, uint8_t w) NONBANKED
{
    fmt_num(lb, v, w);
    put_strn(x, y, lb, w);
}

void fmt_note(char *d, uint8_t note) NONBANKED
{
    static const char nn[] = "C C#D D#E F F#G G#A A#B ";
    uint8_t pc = note % 12, i = 1;
    d[0] = nn[pc * 2];
    if (nn[pc * 2 + 1] == '#') d[i++] = '#';
    d[i++] = (char)('0' + (note / 12) - 1);
    if (i < 3) d[i++] = ' ';
    d[3] = 0;
}

void fmt_time(char *d, uint16_t s, uint8_t h) NONBANKED
{
    uint8_t m = (uint8_t)(s / 60), sec = (uint8_t)(s % 60), i = 0;
    if (h) { d[i++] = (char)('0' + (h % 10)); d[i++] = ':'; }
    d[i++] = (char)('0' + m / 10); d[i++] = (char)('0' + m % 10);
    d[i++] = ':';
    d[i++] = (char)('0' + sec / 10); d[i++] = (char)('0' + sec % 10);
    d[i] = 0;
}

void put_hbar(uint8_t x, uint8_t y, uint8_t w, uint8_t v, uint8_t max) NONBANKED
{
    uint16_t px = max ? (uint16_t)((uint16_t)v * (w * 8) / max) : 0;
    uint8_t i;
    for (i = 0; i < w; i++) {
        if (px >= 8) { tb[i] = T_HBAR + 8; px -= 8; }
        else { tb[i] = (uint8_t)(T_HBAR + px); px = 0; }
    }
    set_bkg_tiles(x, y, w, 1, tb);
}

/* --------------------------------------------------------------- input
 * The VBlank interrupt samples the joypad every frame and latches new
 * presses, so a press is never lost while the main loop is busy (e.g.
 * verifying SRAM).  Auto-repeat for the D-pad is derived in the UI loop. */
static volatile uint8_t joy_pending, joy_isr_prev, joy_isr_now;

void input_isr(void) NONBANKED
{
    uint8_t j = joypad();
    joy_pending |= (uint8_t)(j & ~joy_isr_prev);
    joy_isr_prev = j;
    joy_isr_now = j;
}

uint8_t joy_poll(void) NONBANKED
{
    uint8_t j, ev, dir;
    __critical {
        ev = joy_pending;
        joy_pending = 0;
        j = joy_isr_now;
    }
    dir = j & 0x0F;
    if (dir && dir == (joy_prev & 0x0F)) {
        if (rep_cnt < 255) rep_cnt++;
        if (rep_cnt >= 16 && ((rep_cnt - 16) % 3) == 0) {
            ev |= dir;
            if (joy_accel < 255) joy_accel++;
        }
    } else {
        rep_cnt = 0;
        joy_accel = 0;
    }
    joy_prev = j;
    joy_held = j;
    return ev;
}

/* --------------------------------------------------- help / messages
 * Rows 16-17 form the help panel: two lines describing the selected
 * control.  A message temporarily replaces the second line. */
extern uint8_t ui_panel_single;
void ui_panel_draw(void) NONBANKED
{
    if (ui_panel_single) {
        if (ui_msg_t) { put_tile(0, 17, T_DOTSM); put_low(1, 17, ui_msgbuf, 19); }
        else put_low(0, 17, ui_help_txt, 20);
        return;
    }
    put_low(0, 16, ui_help_txt, 20);
    if (ui_msg_t) {
        put_tile(0, 17, T_DOTSM);
        put_low(1, 17, ui_msgbuf, 19);
    } else put_low(0, 17, ui_help_txt + 21, 20);
}

void ui_msg(const char *s) NONBANKED
{
    uint8_t i;
    for (i = 0; i < 19 && s[i]; i++) ui_msgbuf[i] = s[i];
    ui_msgbuf[i] = 0;
    ui_msg_t = 110;
    ui_panel_draw();
}

void ui_hint(const char *s) NONBANKED
{
    uint8_t i;
    for (i = 0; i < 20 && s[i]; i++) ui_help_txt[i] = s[i];
    ui_help_txt[i] = 0;
    ui_help_txt[21] = 0;
    ui_panel_draw();
}

/* set the help panel to two explicit lines (any bank) */
void ui_help2(const char *a, const char *b) NONBANKED
{
    uint8_t i;
    for (i = 0; i < 20 && a[i]; i++) ui_help_txt[i] = a[i];
    ui_help_txt[i] = 0;
    for (i = 0; i < 20 && b[i]; i++) ui_help_txt[21 + i] = b[i];
    ui_help_txt[21 + i] = 0;
    ui_panel_draw();
}

/* look up help for a label (or an @KEY) and show it */
void ui_help(const char *key) NONBANKED
{
    char k[16];
    uint8_t i = 0;
    if (key[0] >= '1' && key[0] <= '8' && key[1] == ' ') key += 2;   /* "1 SOURCE" */
    for (; i < 15 && key[i]; i++) k[i] = key[i];
    k[i] = 0;
    help_lookup(k, ui_help_txt);
    ui_panel_draw();
}

/* ------------------------------------------------------- param lists */
uint8_t *pd_ptr(const PDesc *d) NONBANKED
{
    switch (d->base) {
    case B_AGENT: return &proj.agent[ui_agent & 7].a[d->off];
    case B_INST:  return &proj.inst[ui_inst & 7].p[d->off];
    case B_HARM:  return &proj.harm[d->off];
    case B_ECO:   return &proj.eco[d->off];
    case B_GLOB:  return &proj.glob[d->off];
    case B_SCENE: return &proj.scene[ui_scene & 7].s[d->off];
    case B_MOD:   return (uint8_t *)&proj.mod[ui_mod & 3] + d->off;
    case B_MACRO: return &proj.macro[d->off];
    case B_MOTIF: return (uint8_t *)&proj.motif[ui_motif & 3] + d->off;
    case B_UI:    return &ui_var[d->off & 7];
    default:      return 0;
    }
}

static const char agent_letters[] = "OBDSTERM";

static void fmt_val(const PDesc *d, uint8_t v, char *o)
{
    uint8_t i;
    memset(o, ' ', 9);
    o[9] = 0;
    switch (d->fmt) {
    case F_NAME:
        if (d->names) { const char *s = d->names[(uint8_t)(v - d->min)]; for (i = 0; i < 9 && s[i]; i++) o[i] = s[i]; }
        break;
    case F_NOTE: fmt_note(o, v); o[3] = ' '; fmt_num(o + 4, v, 3); o[7] = ' '; break;
    case F_SIGNED: {
        int16_t x = (int16_t)v - d->bias;
        o[0] = x < 0 ? '-' : (x > 0 ? '+' : ' ');
        fmt_num(o + 1, (uint16_t)(x < 0 ? -x : x), 3);
        o[4] = ' ';
        break; }
    case F_ONOFF: o[0] = 'O'; o[1] = v ? 'N' : 'F'; o[2] = v ? ' ' : 'F'; break;
    case F_BAR: {
        uint8_t k;
        fmt_num(o, v, 2);
        o[2] = ' ';
        for (k = 0; k < 6; k++) o[3 + k] = (char)(k < (uint8_t)((v * 6 + 7) / (d->max ? d->max : 15)) ? '|' : '.');
        break; }
    case F_BITS:
        for (i = 0; i < 8; i++) o[i] = (v >> i) & 1 ? 'x' : '.';
        break;
    case F_MASK:
        for (i = 0; i < 8; i++) o[i] = (v >> i) & 1 ? agent_letters[i] : '.';
        break;
    case F_INSTR:
        if (v >= NUM_INST) { o[0] = '-'; o[1] = '-'; break; }
        o[0] = (char)('1' + v); o[1] = ' ';
        for (i = 0; i < INST_NAME_LEN; i++) o[2 + i] = proj.inst[v].name[i];
        break;
    case F_ACTION:
        if (d->names) { const char *s = d->names[0]; for (i = 0; i < 9 && s[i]; i++) o[i] = s[i]; }
        break;
    case F_KEEP:
        if (v >= d->max) { o[0] = 'K'; o[1] = 'E'; o[2] = 'E'; o[3] = 'P'; }
        else fmt_num(o, v, 3);
        break;
    case F_SEC4: fmt_num(o, (uint16_t)v * 4, 3); o[3] = 's'; break;
    case F_LEN15: fmt_time(o, (uint16_t)v * 15, 0); o[5] = ' '; break;
    case F_TEMPO0:
        if (v < 30) { o[0] = 'K'; o[1] = 'E'; o[2] = 'E'; o[3] = 'P'; }
        else { fmt_num(o, v, 3); o[3] = ' '; o[4] = 'B'; o[5] = 'P'; o[6] = 'M'; }
        break;
    case F_LEN30: fmt_time(o, (uint16_t)v * 30, 0); o[5] = ' '; break;
    case F_VOL:
        if (!v) { o[0] = 'P'; o[1] = 'A'; o[2] = 'T'; o[3] = 'C'; o[4] = 'H'; }
        else fmt_num(o, v, 3);
        break;
    case F_DUR:
    case F_RATE: {
        uint16_t fr;
        if (d->fmt == F_RATE && proj.glob[GP_TMODE] == TM_CLOCKED) {
            fmt_num(o, rate_steps(v), 3); o[3] = ' '; o[4] = 'S'; o[5] = 'T'; o[6] = 'E'; o[7] = 'P';
            break;
        }
        fr = d->fmt == F_DUR ? dur_frames(v) : rate_frames(v);
        fr = (uint16_t)((fr + 3) / 6);              /* tenths of a second */
        fmt_num(o, fr / 10, 3);
        o[3] = '.'; o[4] = (char)('0' + fr % 10); o[5] = 's';
        break; }
    default: fmt_num(o, (uint16_t)((int16_t)v - d->bias), 3); break;
    }
    for (i = 0; i < 9; i++) if (!o[i]) o[i] = ' ';
}

static uint8_t keep_case(uint8_t fmt)
{
    return fmt == F_NOTE || fmt == F_MASK;
}

/* row layout:  [cursor][label 9][<][value 8][>]
 * the selected row shows a cursor and arrows around adjustable values */
void list_row(const PDesc *t, uint8_t i, uint8_t y, uint8_t sel) NONBANKED
{
    const PDesc *d = &t[i];
    uint8_t *p = pd_ptr(d);
    uint8_t adj;
    char v[10];
    put_tile(0, y, sel ? T_CURSOR : 0);
    put_low(1, y, d->label, 9);
    adj = (uint8_t)(sel && p && d->fmt != F_ACTION);
    put_tile(10, y, adj && d->fmt != F_MASK && d->fmt != F_BITS ? T_LARROW : 0);
    put_tile(19, y, adj && d->fmt != F_MASK && d->fmt != F_BITS ? T_RARROW : 0);
    if (d->fmt == F_BAR && p) {
        /* numeric value followed by a small meter */
        put_num(11, y, *p, 2);
        put_tile(13, y, 0);
        put_hbar(14, y, 5, *p, d->max ? d->max : 15);
        return;
    }
    fmt_val(d, (d->fmt == F_ACTION || !p) ? 0 : *p, v);
    v[8] = 0;
    if (keep_case(d->fmt)) put_strn(11, y, v, 8); else put_low(11, y, v, 8);
    if (sel && p && (d->fmt == F_MASK || d->fmt == F_BITS)) {
        lb[0] = v[ui_sub & 7]; lb[1] = 0;
        if (lb[0] == '.') lb[0] = '_';
        put_inv((uint8_t)(11 + (ui_sub & 7)), y, lb, 1);
    }
}

static void scroll_marks(uint8_t n, uint8_t y0, uint8_t vis, uint8_t top)
{
    put_tile(0, (uint8_t)(y0 - 1), top > 0 ? T_UP : T_RULE);
    put_tile(0, (uint8_t)(y0 + vis), top + vis < n ? T_DOWN : T_RULE);
}

void list_draw(const PDesc *t, uint8_t n, uint8_t y0, uint8_t vis, uint8_t cur, uint8_t top) NONBANKED
{
    uint8_t r;
    for (r = 0; r < vis; r++) {
        uint8_t i = (uint8_t)(top + r);
        if (i < n) list_row(t, i, (uint8_t)(y0 + r), i == cur);
        else put_strn(0, (uint8_t)(y0 + r), "", 20);
    }
    if (n > vis) scroll_marks(n, y0, vis, top);
    if (cur < n) ui_help(t[cur].label);
}

uint8_t list_input(const PDesc *t, uint8_t n, uint8_t *cur, uint8_t *top, uint8_t vis, uint8_t keys) NONBANKED
{
    const PDesc *d = &t[*cur];
    uint8_t *p;
    if (keys & K_UP) {
        ui_sub = 0;
        if (*cur == 0) return LI_NONE;
        (*cur)--;
        if (*cur < *top) { *top = *cur; return LI_SCROLL; }
        return LI_MOVED;
    }
    if (keys & K_DOWN) {
        ui_sub = 0;
        if (*cur + 1 >= n) return LI_NONE;
        (*cur)++;
        if (*cur >= *top + vis) { *top = (uint8_t)(*cur - vis + 1); return LI_SCROLL; }
        return LI_MOVED;
    }
    if (keys & K_B) return LI_BACK;
    p = pd_ptr(d);
    if (d->fmt == F_MASK || d->fmt == F_BITS) {
        if (keys & K_LEFT) { ui_sub = (uint8_t)((ui_sub + 7) & 7); return LI_CHANGED; }
        if (keys & K_RIGHT) { ui_sub = (uint8_t)((ui_sub + 1) & 7); return LI_CHANGED; }
        if ((keys & K_A) && p) { *p ^= (uint8_t)(1 << (ui_sub & 7)); return LI_CHANGED; }
        return LI_NONE;
    }
    if (keys & K_A) return LI_ACTION;
    if (!p || d->fmt == F_ACTION) return LI_NONE;
    if (keys & (K_LEFT | K_RIGHT)) {
        uint8_t range = (uint8_t)(d->max - d->min);
        uint8_t step = 1, v = *p;
        if (joy_accel > 10 && range > 40) step = (uint8_t)(range >> 4);
        if (keys & K_RIGHT) v = (uint8_t)((uint16_t)v + step > d->max ? d->max : v + step);
        else v = (uint8_t)((int16_t)v - step < (int16_t)d->min ? d->min : v - step);
        if (v != *p) { *p = v; return LI_CHANGED; }
    }
    return LI_NONE;
}

uint8_t list_run(const PDesc *t, uint8_t n, uint8_t y0, uint8_t vis, uint8_t *cur, uint8_t *top, uint8_t keys) NONBANKED
{
    uint8_t old = *cur;
    uint8_t r = list_input(t, n, cur, top, vis, keys);
    if (r == LI_SCROLL) list_draw(t, n, y0, vis, *cur, *top);
    else if (r == LI_MOVED) {
        list_row(t, old, (uint8_t)(y0 + old - *top), 0);
        list_row(t, *cur, (uint8_t)(y0 + *cur - *top), 1);
        if (n > vis) scroll_marks(n, y0, vis, *top);
        ui_help(t[*cur].label);
    } else if (r == LI_CHANGED) list_row(t, *cur, (uint8_t)(y0 + *cur - *top), 1);
    return r;
}

/* ---------------------------------------------------------- text edit */
static const char charset[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-.#&/!?'";

uint8_t text_edit(char *buf, uint8_t len, uint8_t x, uint8_t y, uint8_t keys, uint8_t *pos) NONBANKED
{
    uint8_t i, k = 0;
    if (keys & (K_A | K_B)) { put_strn(x, y, buf, len); return 0; }
    if (!keys) ui_help("@TEXTEDIT");
    if (keys & K_LEFT && *pos) (*pos)--;
    if (keys & K_RIGHT && *pos + 1 < len) (*pos)++;
    if (keys & (K_UP | K_DOWN)) {
        uint8_t n = (uint8_t)(sizeof(charset) - 1);
        for (i = 0; i < n; i++) if (charset[i] == buf[*pos]) { k = i; break; }
        if (keys & K_UP) k = (uint8_t)((k + 1) % n);
        else k = (uint8_t)((k + n - 1) % n);
        buf[*pos] = charset[k];
    }
    put_strn(x, y, buf, len);
    lb[0] = buf[*pos] == ' ' ? '_' : buf[*pos]; lb[1] = 0;
    put_inv((uint8_t)(x + *pos), y, lb, 1);
    return 1;
}

/* -------------------------------------------------------------- header
 * Lives in bank 0 so that every banked page can pass its own string. */
extern uint8_t ui_page;

static const char * const page_names[9] = {
    "aether", "sounds", "players", "key", "sections", "save/load", "menu", "songs", "fine tune"
};

/* header: orbit mark, page name and an optional sub-title in lowercase,
 * play state at the right; row 1 is a dotted rule carrying the page
 * dots; row 15 is the rule above the help panel. */
void ui_header(const char *sub) NONBANKED
{
    uint8_t i, x;
    put_strn(0, 0, "", 20);
    put_tile(0, 0, T_ORB);
    x = 2;
    put_low(x, 0, page_names[ui_page], 9);
    for (i = 0; page_names[ui_page][i]; i++) x++;
    if (sub) {
        put_strn((uint8_t)(x + 1), 0, ">", 1);
        put_low((uint8_t)(x + 3), 0, sub, (uint8_t)(16 - x));
    }
    put_tile(19, 0, eng.playing ? T_PLAY : T_STOP);
    put_rule(1);
    put_rule(15);
}
