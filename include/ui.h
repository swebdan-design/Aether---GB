/*
 * AETHER - by Synth Centric
 * ui.h : page framework.
 */
#ifndef AETHER_UI_H
#define AETHER_UI_H
#include "gfx.h"

#define PG_LIVE    0
#define PG_SYNTH   1
#define PG_AGENTS  2
#define PG_HARMONY 3
#define PG_FORM    4
#define PG_PROJECT 5
#define PG_MENU    6
#define PG_SONGS   7
#define PG_TUNE    8
#define NUM_PAGES  9

#define M_KEYS  0     /* handle key events          */
#define M_DRAW  1     /* full redraw, keep state    */
#define M_TICK  2     /* periodic live update        */
#define M_ENTER 3     /* page selected: reset + draw */

extern uint8_t ui_page;
extern uint8_t ui_frame_cnt;
extern uint8_t imprint_next;

void ui_init(void) BANKED;
void ui_title(void) BANKED;            /* blocking title screen */
void ui_frame(void) BANKED;            /* one UI frame          */
void ui_header(const char *sub) NONBANKED;   /* bank 0: takes strings from any bank */
void ui_redraw(void) BANKED;
void ui_toggle_play(void) BANKED;
void ui_home(void) BANKED;              /* back to the menu */
void ui_goto(uint8_t page) BANKED;
extern uint8_t ui_panel_single;

void pg_live(uint8_t keys, uint8_t mode) BANKED;
void pg_synth(uint8_t keys, uint8_t mode) BANKED;
void pg_agents(uint8_t keys, uint8_t mode) BANKED;
void pg_harmony(uint8_t keys, uint8_t mode) BANKED;
void pg_form(uint8_t keys, uint8_t mode) BANKED;
void pg_project(uint8_t keys, uint8_t mode) BANKED;
void pg_menu(uint8_t keys, uint8_t mode) BANKED;
void pg_songs(uint8_t keys, uint8_t mode) BANKED;
void pg_tune(uint8_t keys, uint8_t mode) BANKED;

extern uint8_t dynbuf[64 * 16];

/* shared helpers implemented in ui.c */
uint8_t ui_capture_imprint(uint8_t agent_filter) BANKED;
void    ui_scene_label(uint8_t s, char *out) BANKED;   /* "S1 ARRIVAL" (11 chars) */

/* profiling (main.c) */
extern uint8_t prof_gen_lines, prof_ui_lines, prof_gen_max, prof_ui_max;
#endif
