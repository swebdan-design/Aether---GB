/*
 * AETHER - by Synth Centric
 * ui_harmony.c : HARMONY page - scale, tonal centre, harmonic behaviour,
 * interval weighting, user pitch-class set, live harmonic memory view.
 */
#pragma bank 255
#include <gb/gb.h>
#include <string.h>
#include "ui.h"
#include "engine.h"

#define LV_TOP 0
#define LV_IW  1
#define LV_USER 2

static uint8_t level, cur, top, sub_cur, sub_top;

static const char * const root_names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
static const char * const scale_names[NUM_SCALES] = {
    "MAJOR", "MINOR", "MAJ PENT", "MIN PENT", "DORIAN", "PHRYGIAN", "LYDIAN", "MIXOLYD", "CHROMATIC", "USER SET"
};
static const char * const mm_names[3] = { "REINFORCE", "EXPLORE", "DRIFT" };
static const char * const open_name[] = { "EDIT" };
static const char * const roman[7] = { "I", "II", "III", "IV", "V", "VI", "VII" };

static const PDesc harm_tab[] = {
    { "ROOT",      B_HARM, HP_ROOT, 0, 11, F_NAME, 0, root_names },
    { "SCALE",     B_HARM, HP_SCALE, 0, NUM_SCALES - 1, F_NAME, 0, scale_names },
    { "LOWEST",    B_HARM, HP_LO, 24, 100, F_NOTE, 0, 0 },
    { "HIGHEST",   B_HARM, HP_HI, 30, 107, F_NOTE, 0, 0 },
    { "CONSONANT", B_HARM, HP_CONS, 0, 15, F_BAR, 0, 0 },
    { "DISSONANT", B_HARM, HP_DISS, 0, 15, F_BAR, 0, 0 },
    { "CHANGE",    B_HARM, HP_CHANGE, 0, 15, F_NUM, 0, 0 },
    { "CENTRE",    B_HARM, HP_CSTAB, 0, 15, F_NUM, 0, 0 },
    { "RESOLVE",   B_HARM, HP_RESOL, 0, 15, F_NUM, 0, 0 },
    { "SEPARATE",  B_HARM, HP_REGSEP, 0, 12, F_NUM, 0, 0 },
    { "MEM MODE",  B_HARM, HP_MMODE, 0, 2, F_NAME, 0, mm_names },
    { "MEM POWER", B_HARM, HP_MSTR, 0, 15, F_NUM, 0, 0 },
    { "INTERVALS", B_NONE, 0, 0, 0, F_ACTION, 0, open_name },
    { "USER SET",  B_NONE, 0, 0, 0, F_ACTION, 0, open_name },
};
#define N_HARM (sizeof(harm_tab) / sizeof(PDesc))

static const PDesc iw_tab[] = {
    { "UNISON",    B_HARM, HP_IW + 0, 0, 15, F_BAR, 0, 0 },
    { "MIN 2ND",   B_HARM, HP_IW + 1, 0, 15, F_BAR, 0, 0 },
    { "MAJ 2ND",   B_HARM, HP_IW + 2, 0, 15, F_BAR, 0, 0 },
    { "MIN 3RD",   B_HARM, HP_IW + 3, 0, 15, F_BAR, 0, 0 },
    { "MAJ 3RD",   B_HARM, HP_IW + 4, 0, 15, F_BAR, 0, 0 },
    { "4TH",       B_HARM, HP_IW + 5, 0, 15, F_BAR, 0, 0 },
    { "TRITONE",   B_HARM, HP_IW + 6, 0, 15, F_BAR, 0, 0 },
    { "5TH",       B_HARM, HP_IW + 7, 0, 15, F_BAR, 0, 0 },
    { "MIN 6TH",   B_HARM, HP_IW + 8, 0, 15, F_BAR, 0, 0 },
    { "MAJ 6TH",   B_HARM, HP_IW + 9, 0, 15, F_BAR, 0, 0 },
    { "MIN 7TH",   B_HARM, HP_IW + 10, 0, 15, F_BAR, 0, 0 },
    { "MAJ 7TH",   B_HARM, HP_IW + 11, 0, 15, F_BAR, 0, 0 },
};
#define N_IW 12

static void memory_view(void)
{
    uint8_t i, t[20], mx = 1;
    put_strn(0, 14, "KEY", 3);
    put_strn(4, 14, root_names[eng.hs.root], 2);
    put_strn(7, 14, "CHORD", 5);
    put_strn(13, 14, roman[eng.hs.degree % 7], 3);
    put_strn(17, 14, eng.eco.resolving ? "RES" : "   ", 3);
    for (i = 0; i < 12; i++) if (eng.hs.pc_hist[i] > mx) mx = eng.hs.pc_hist[i];
    memset(t, 0, 20);
    for (i = 0; i < 12; i++) {
        uint8_t v = (uint8_t)(((uint16_t)eng.hs.pc_hist[i] * 8) / mx);
        t[4 + i] = (uint8_t)(T_VBAR + v);
    }
    put_tiles(0, 15, 20, t);
    put_strn(0, 15, "MEM", 3);
    memset(t, 0, 20);
    for (i = 0; i < 12; i++) {
        uint8_t c = (uint8_t)(root_names[i][1] == '#' ? '#' : root_names[i][0]);
        t[4 + i] = (uint8_t)(c - 32);
        if (eng.hs.mask & (1u << i)) t[4 + i] = (uint8_t)(T_INV + c - 32);
    }
    put_tiles(0, 16, 20, t);
}

static void draw_top(void)
{
    gfx_cls();
    ui_header(0);
    list_draw(harm_tab, N_HARM, 1, 12, cur, top);
    memory_view();
    ui_hint("A:OPEN  B:BACK");
}

static void draw_iw(void)
{
    gfx_cls();
    ui_header("INTERVAL");
    put_strn(1, 1, "WEIGHT VS CHORD ROOT", 20);
    list_draw(iw_tab, N_IW, 2, 12, sub_cur, sub_top);
    ui_hint("B:BACK");
}

static void user_draw(void)
{
    uint8_t i;
    uint16_t m = (uint16_t)(proj.harm[HP_UMASKL] | ((uint16_t)proj.harm[HP_UMASKH] << 8));
    for (i = 0; i < 12; i++) {
        uint8_t x = (uint8_t)(1 + (i % 6) * 3), y = (uint8_t)(4 + (i / 6) * 3);
        put_tile(x, (uint8_t)(y - 1), i == sub_cur ? T_CURSOR : 0);
        put_strn((uint8_t)(x + 1), (uint8_t)(y - 1), root_names[i], 2);
        put_tile((uint8_t)(x + 1), y, (m >> i) & 1 ? T_BOX_ON : T_BOX_OFF);
    }
}

static void draw_user(void)
{
    gfx_cls();
    ui_header("USER SET");
    put_strn(1, 1, "PITCH CLASSES", 13);
    user_draw();
    put_strn(1, 10, "INTERVALS ABOVE THE", 19);
    put_strn(1, 11, "ROOT. SELECT SCALE", 19);
    put_strn(1, 12, "'USER SET' TO USE.", 19);
    ui_hint("A:TOGGLE B:BACK");
}

void pg_harmony(uint8_t keys, uint8_t mode) BANKED
{
    if (mode == M_ENTER) { level = LV_TOP; mode = M_DRAW; }
    if (mode == M_DRAW) {
        if (level == LV_IW) draw_iw();
        else if (level == LV_USER) draw_user();
        else draw_top();
        return;
    }
    if (mode == M_TICK) {
        if (level == LV_TOP && (ui_frame_cnt & 15) == 0) memory_view();
        return;
    }
    if (level == LV_TOP) {
        uint8_t r = list_run(harm_tab, N_HARM, 1, 12, &cur, &top, keys);
        if (r == LI_CHANGED && cur <= 1) harm_refresh();
        if (r == LI_CHANGED && cur == 2 && proj.harm[HP_LO] + 6 > proj.harm[HP_HI]) proj.harm[HP_HI] = (uint8_t)(proj.harm[HP_LO] + 6);
        if (r == LI_ACTION && cur == N_HARM - 2) { level = LV_IW; sub_cur = 0; sub_top = 0; draw_iw(); }
        if (r == LI_ACTION && cur == N_HARM - 1) { level = LV_USER; sub_cur = 0; draw_user(); }
    } else if (level == LV_IW) {
        if (list_run(iw_tab, N_IW, 2, 12, &sub_cur, &sub_top, keys) == LI_BACK) { level = LV_TOP; draw_top(); }
    } else {
        if (keys & K_B) { level = LV_TOP; draw_top(); return; }
        if (keys & K_LEFT) sub_cur = (uint8_t)((sub_cur + 11) % 12);
        if (keys & K_RIGHT) sub_cur = (uint8_t)((sub_cur + 1) % 12);
        if (keys & K_UP) sub_cur = (uint8_t)((sub_cur + 6) % 12);
        if (keys & K_DOWN) sub_cur = (uint8_t)((sub_cur + 6) % 12);
        if (keys & K_A) {
            uint16_t m = (uint16_t)(proj.harm[HP_UMASKL] | ((uint16_t)proj.harm[HP_UMASKH] << 8));
            m ^= (uint16_t)(1u << sub_cur);
            m |= 1;                                   /* root always present */
            proj.harm[HP_UMASKL] = (uint8_t)m;
            proj.harm[HP_UMASKH] = (uint8_t)((m >> 8) & 15);
            harm_refresh();
        }
        user_draw();
    }
}
