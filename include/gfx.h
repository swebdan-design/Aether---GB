/*
 * AETHER - by Synth Centric
 * gfx.h : text, glyphs, meters, input (fixed bank 0) and the generic
 * table-driven parameter list used by every editor page.
 */
#ifndef AETHER_GFX_H
#define AETHER_GFX_H
#include "aether.h"

/* ---------------------------------------------------------- tile map */
#define T_INV       95     /* inverted font: T_INV + (c - 32), c in 32..95 */
#define T_HBAR      160    /* 9 tiles 0/8 .. 8/8 */
#define T_VBAR      169    /* 9 tiles */
#define T_DOT_ON    178
#define T_DOT_OFF   179
#define T_DOT_DIM   180
#define T_PLAY      181
#define T_STOP      182
#define T_CURSOR    183
#define T_LARROW    184
#define T_RARROW    185
#define T_HLINE     186
#define T_PAGE_ON   187
#define T_PAGE_OFF  188
#define T_NOTE      189
#define T_BOX_ON    190
#define T_BOX_OFF   191
#define T_DYN       192    /* 64 dynamic tiles (trace, waveform, logo) */

/* -------------------------------------------------------------- keys */
#define K_RIGHT  0x01
#define K_LEFT   0x02
#define K_UP     0x04
#define K_DOWN   0x08
#define K_A      0x10
#define K_B      0x20
#define K_SELECT 0x40
#define K_START  0x80

extern uint8_t joy_held;
extern uint8_t joy_accel;          /* number of auto-repeats so far */
uint8_t joy_poll(void) NONBANKED;  /* edge + auto-repeat events     */
void    input_isr(void) NONBANKED; /* VBlank: latch joypad presses  */

/* -------------------------------------------------------------- text */
extern char lb[24];                /* scratch line buffer           */
void gfx_init(void) NONBANKED;
void gfx_title_tiles(void) NONBANKED;
void gfx_cls(void) NONBANKED;
void gfx_clear_rows(uint8_t y0, uint8_t y1) NONBANKED;
void put_tile(uint8_t x, uint8_t y, uint8_t t) NONBANKED;
void put_tiles(uint8_t x, uint8_t y, uint8_t n, const uint8_t *t) NONBANKED;
void put_str(uint8_t x, uint8_t y, const char *s) NONBANKED;
void put_strn(uint8_t x, uint8_t y, const char *s, uint8_t n) NONBANKED;
void put_inv(uint8_t x, uint8_t y, const char *s, uint8_t n) NONBANKED;
void put_num(uint8_t x, uint8_t y, uint16_t v, uint8_t w) NONBANKED;
void put_hbar(uint8_t x, uint8_t y, uint8_t w, uint8_t v, uint8_t max) NONBANKED;
void fmt_num(char *d, uint16_t v, uint8_t w) NONBANKED;      /* right aligned */
void fmt_note(char *d, uint8_t note) NONBANKED;             /* 3 chars "C#4" */
void fmt_time(char *d, uint16_t s, uint8_t h) NONBANKED;     /* "h:mm:ss" / "mm:ss" */
void dyn_tile(uint8_t idx, const uint8_t *data) NONBANKED;   /* upload 16 bytes */

/* -------------------------------------------------- parameter lists */
#define B_NONE   0
#define B_AGENT  1
#define B_INST   2
#define B_HARM   3
#define B_ECO    4
#define B_GLOB   5
#define B_SCENE  6
#define B_MOD    7
#define B_MACRO  8
#define B_MOTIF  9
#define B_UI    10       /* ui_var[] scratch values */

#define F_NUM    0
#define F_NAME   1       /* names[v - min]                        */
#define F_NOTE   2
#define F_SIGNED 3       /* shows v - bias with sign              */
#define F_ONOFF  4
#define F_BAR    5
#define F_BITS   6       /* 8 bit pattern edited bit by bit       */
#define F_INSTR  7
#define F_ACTION 8       /* names[0] shown, A triggers            */
#define F_MASK   9       /* agent mask letters                    */
#define F_KEEP   10      /* numeric, value == max shows "KEEP"    */
#define F_SEC4   11      /* value * 4 seconds                     */
#define F_LEN15  12      /* value * 15 seconds as m:ss            */
#define F_TEMPO0 13      /* < 30 shows KEEP                       */
#define F_LEN30  14      /* value * 30 seconds as m:ss            */
#define F_VOL    15      /* 0 shows PATCH                         */
#define F_DUR    16      /* duration table index shown in seconds */
#define F_RATE   17      /* interval index: seconds or steps      */

typedef struct {
    const char *label;
    uint8_t base, off, min, max, fmt;
    int8_t  bias;
    const char * const *names;
} PDesc;

extern uint8_t ui_agent, ui_inst, ui_scene, ui_mod, ui_motif, ui_sub;
extern uint8_t ui_var[8];

#define LI_NONE    0
#define LI_MOVED   1
#define LI_CHANGED 2
#define LI_SCROLL  3
#define LI_ACTION  4
#define LI_BACK    5

uint8_t *pd_ptr(const PDesc *d) NONBANKED;
void    list_row(const PDesc *t, uint8_t i, uint8_t y, uint8_t sel) NONBANKED;
void    list_draw(const PDesc *t, uint8_t n, uint8_t y0, uint8_t vis, uint8_t cur, uint8_t top) NONBANKED;
uint8_t list_input(const PDesc *t, uint8_t n, uint8_t *cur, uint8_t *top, uint8_t vis, uint8_t keys) NONBANKED;
/* convenience: input + minimal redraw; returns LI_* */
uint8_t list_run(const PDesc *t, uint8_t n, uint8_t y0, uint8_t vis, uint8_t *cur, uint8_t *top, uint8_t keys) NONBANKED;

/* single line text editor; returns 1 while editing, 0 when done */
uint8_t text_edit(char *buf, uint8_t len, uint8_t x, uint8_t y, uint8_t keys, uint8_t *pos) NONBANKED;

/* status / hint line */
void ui_msg(const char *s) NONBANKED;
void ui_hint(const char *s) NONBANKED;
extern uint8_t ui_msg_t;

#endif
