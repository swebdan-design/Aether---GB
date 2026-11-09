/*
 * AETHER - by Synth Centric
 * ui_form.c : FORM page - timing mode, FOREVER, scenes (list, editor,
 * store), and the IMPRINT motif library with manual note entry.
 */
#pragma bank 255
#include <gb/gb.h>
#include <string.h>
#include "ui.h"
#include "engine.h"

#define LV_TOP    0
#define LV_SCENES 1
#define LV_SCENE  2
#define LV_MOTIFS 3
#define LV_MOTIF  4
#define LV_NOTES  5

static uint8_t level, cur, top, sub_cur, sub_top, naming, npos, ncol, confirm;

static const char * const tm_names[] = { "CLOCKED", "FREE" };
static const char * const tm3_names[] = { "CLOCKED", "FREE", "KEEP" };
static const char * const scn_names[] = { "S1", "S2", "S3", "S4", "S5", "S6", "S7", "S8" };
static const char * const act_go[] = { "a: go" };
static const char * const act_open[] = { "a: open" };
static const char * const act_store[] = { "a: store" };
static const char * const act_rename[] = { "a: name" };
static const char * const root13[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B", "KEEP" };
static const char * const scale11[] = { "MAJOR", "MINOR", "MAJ PENT", "MIN PENT", "DORIAN", "PHRYGIAN", "LYDIAN", "MIXOLYD", "CHROMATIC", "USER SET", "KEEP" };
static const char * const trans_names[] = { "MANUAL", "TIME", "CHANCE", "ECOLOGY" };
static const char * const next_names[] = { "S1", "S2", "S3", "S4", "S5", "S6", "S7", "S8", "NEXT", "RANDOM", "END" };
static const char * const hm_names[] = { "REINFORCE", "EXPLORE", "DRIFT", "KEEP" };
static const char * const agent9[] = { "ORBIT", "BLOOM", "DRIFT", "SCATTER", "TIDE", "ECHO", "ERODE", "MEMORY", "NONE" };
static const char * const cap9[] = { "ORBIT", "BLOOM", "DRIFT", "SCATTER", "TIDE", "ECHO", "ERODE", "MEMORY", "ANY" };
static const char * const tsc_names[] = { "x0.5", "x1", "x1.5", "x2" };
static const char * const a_play[] = { "a: play" };
static const char * const a_mut[] = { "a: vary" };
static const char * const a_thin[] = { "a: thin" };
static const char * const a_fill[] = { "a: fill" };
static const char * const a_cap[] = { "a: grab" };
static const char * const a_clear[] = { "a: clear" };

static const PDesc top_tab[] = {
    { "TIMING",    B_GLOB, GP_TMODE, 0, 1, F_NAME, 0, tm_names },
    { "TEMPO",     B_GLOB, GP_TEMPO, 30, 200, F_NUM, 0, 0 },
    { "FOREVER",   B_GLOB, GP_FOREVER, 0, 1, F_ONOFF, 0, 0 },
    { "START AT",  B_GLOB, GP_START, 0, 7, F_NAME, 0, scn_names },
    { "GO TO",     B_UI, 0, 0, 7, F_NAME, 0, scn_names },
    { "STORE TO",  B_UI, 1, 0, 7, F_NAME, 0, scn_names },
    { "SCENES",    B_NONE, 0, 0, 0, F_ACTION, 0, act_open },
    { "MOTIFS",    B_NONE, 0, 0, 0, F_ACTION, 0, act_open },
};
#define N_TOP (sizeof(top_tab) / sizeof(PDesc))

static const PDesc scene_tab[] = {
    { "NAME",      B_NONE, 0, 0, 0, F_ACTION, 0, act_rename },
    { "ENABLED",   B_SCENE, SC_ON, 0, 1, F_ONOFF, 0, 0 },
    { "AGENTS",    B_SCENE, SC_MASK, 0, 255, F_MASK, 0, 0 },
    { "ROOT",      B_SCENE, SC_ROOT, 0, 12, F_NAME, 0, root13 },
    { "SCALE",     B_SCENE, SC_SCALE, 0, 10, F_NAME, 0, scale11 },
    { "DENSITY",   B_SCENE, SC_MAC + 0, 0, 16, F_KEEP, 0, 0 },
    { "EVOLUTION", B_SCENE, SC_MAC + 1, 0, 16, F_KEEP, 0, 0 },
    { "HARMONY",   B_SCENE, SC_MAC + 2, 0, 16, F_KEEP, 0, 0 },
    { "MOTION",    B_SCENE, SC_MAC + 3, 0, 16, F_KEEP, 0, 0 },
    { "TEXTURE",   B_SCENE, SC_MAC + 4, 0, 16, F_KEEP, 0, 0 },
    { "SPACE",     B_SCENE, SC_MAC + 5, 0, 16, F_KEEP, 0, 0 },
    { "TIMING",    B_SCENE, SC_TMODE, 0, 2, F_NAME, 0, tm3_names },
    { "TEMPO",     B_SCENE, SC_TEMPO, 0, 200, F_TEMPO0, 0, 0 },
    { "ENERGY",    B_SCENE, SC_ENERGY, 0, 15, F_BAR, 0, 0 },
    { "TENSION",   B_SCENE, SC_TENSION, 0, 15, F_BAR, 0, 0 },
    { "STABILITY", B_SCENE, SC_STAB, 0, 15, F_BAR, 0, 0 },
    { "MEMORY",    B_SCENE, SC_MEM, 0, 15, F_BAR, 0, 0 },
    { "LENGTH",    B_SCENE, SC_LEN, 1, 60, F_LEN15, 0, 0 },
    { "CHANGE BY", B_SCENE, SC_TRANS, 0, 3, F_NAME, 0, trans_names },
    { "CHANGE P",  B_SCENE, SC_TPARAM, 0, 15, F_NUM, 0, 0 },
    { "NEXT",      B_SCENE, SC_NEXT, 0, 10, F_NAME, 0, next_names },
    { "GLIDE",     B_SCENE, SC_GLIDE, 0, 15, F_SEC4, 0, 0 },
    { "HARM MODE", B_SCENE, SC_HMODE, 0, 3, F_NAME, 0, hm_names },
};
#define N_SCENE (sizeof(scene_tab) / sizeof(PDesc))

static const PDesc motif_tab[] = {
    { "PLAY",      B_NONE, 0, 0, 0, F_ACTION, 0, a_play },
    { "MUTATE",    B_NONE, 0, 0, 0, F_ACTION, 0, a_mut },
    { "BASE NOTE", B_MOTIF, 1, 24, 100, F_NOTE, 0, 0 },
    { "TIME",      B_MOTIF, 3, 1, 4, F_NAME, 0, tsc_names },
    { "THIN",      B_NONE, 0, 0, 0, F_ACTION, 0, a_thin },
    { "FILL",      B_NONE, 0, 0, 0, F_ACTION, 0, a_fill },
    { "INSTR",     B_MOTIF, 2, 0, 7, F_INSTR, 0, 0 },
    { "RECUR",     B_MOTIF, 4, 0, 15, F_BAR, 0, 0 },
    { "SOURCE OF", B_MOTIF, 5, 0, 8, F_NAME, 0, agent9 },
    { "FROM",      B_UI, 2, 0, 8, F_NAME, 0, cap9 },
    { "CAPTURE",   B_NONE, 0, 0, 0, F_ACTION, 0, a_cap },
    { "NOTES",     B_NONE, 0, 0, 0, F_ACTION, 0, act_open },
    { "CLEAR",     B_NONE, 0, 0, 0, F_ACTION, 0, a_clear },
};
#define N_MOTIF (sizeof(motif_tab) / sizeof(PDesc))

/* ----------------------------------------------------------------- top */
static void top_status(void)
{
    char s[12];
    uint16_t len = (uint16_t)(eng.form.live.s[SC_LEN] ? eng.form.live.s[SC_LEN] : 8) * 15;
    ui_scene_label(eng.form.scene, s);
    put_low(1, 11, "now", 3);
    put_low(5, 11, s, 11);
    fmt_time(lb, eng.form.elapsed, 0);
    put_strn(1, 12, lb, 5);
    put_strn(6, 12, "/", 1);
    fmt_time(lb, len, 0);
    put_strn(7, 12, lb, 5);
    put_low(13, 12, trans_names[eng.form.live.s[SC_TRANS] & 3], 7);
    put_low(1, 13, "changes", 7);
    put_num(9, 13, eng.form.transitions, 3);
    put_low(13, 13, eng.form.ending ? "ending" : (proj.glob[GP_FOREVER] ? "forevr" : "      "), 6);
}

static void draw_top(void)
{
    gfx_cls();
    ui_header(0);
    list_draw(top_tab, N_TOP, 2, N_TOP, cur, 0);
    top_status();
}

/* -------------------------------------------------------------- scenes */
static void scenes_row(uint8_t i)
{
    char s[12];
    uint8_t y = (uint8_t)(2 + i);
    put_tile(0, y, cur == i ? T_CURSOR : 0);
    ui_scene_label(i, s);
    put_low(1, y, s, 11);
    put_tile(13, y, proj.scene[i].s[SC_ON] ? T_BOX_ON : T_BOX_OFF);
    put_low(15, y, trans_names[proj.scene[i].s[SC_TRANS] & 3], 4);
    put_tile(19, y, eng.form.scene == i ? T_PLAY : 0);
}

static void draw_scenes(void)
{
    uint8_t i;
    gfx_cls();
    ui_header("scenes");
    for (i = 0; i < NUM_SCENES; i++) scenes_row(i);
    put_low(1, 11, "scenes are states of", 19);
    put_low(1, 12, "behaviour, not notes", 19);
    ui_help("@SCENES");
}

static void draw_scene(void)
{
    char s[12];
    gfx_cls();
    ui_scene_label(ui_scene, s);
    ui_header(s);
    list_draw(scene_tab, N_SCENE, 2, 13, sub_cur, sub_top);
}

/* -------------------------------------------------------------- motifs */
static void motifs_row(uint8_t i)
{
    Motif *m = &proj.motif[i];
    uint8_t y = (uint8_t)(2 + i * 2);
    put_tile(0, y, cur == i ? T_CURSOR : 0);
    lb[0] = 'M'; lb[1] = (char)('1' + i); lb[2] = 0;
    put_low(1, y, lb, 2);
    if (!m->len) { put_low(4, y, "empty", 16); put_strn(4, (uint8_t)(y + 1), "", 16); return; }
    put_num(4, y, m->len, 1);
    put_low(6, y, "notes", 5);
    fmt_note(lb, m->base);
    put_strn(12, y, lb, 3);
    put_low(16, y, "r", 1);
    put_num(17, y, m->recur, 2);
    put_low(4, (uint8_t)(y + 1), "src", 3);
    put_low(8, (uint8_t)(y + 1), agent9[m->source > 8 ? 8 : m->source], 8);
}

static void draw_motifs(void)
{
    uint8_t i;
    gfx_cls();
    ui_header("imprint");
    for (i = 0; i < NUM_MOTIFS; i++) motifs_row(i);
    put_low(1, 11, "on live, a captures", 19);
    put_low(1, 12, "the recent phrase", 19);
    ui_help("@MOTIFS");
}

static void draw_motif(void)
{
    gfx_cls();
    lb[0] = 'M'; lb[1] = (char)('1' + ui_motif); lb[2] = ' ';
    lb[3] = (char)('0' + proj.motif[ui_motif].len); lb[4] = 'N'; lb[5] = 0;
    ui_header(lb);
    list_draw(motif_tab, N_MOTIF, 2, N_MOTIF, sub_cur, 0);
}

/* ---------------------------------------------------------- note table */
static void notes_row(uint8_t r)
{
    Motif *m = &proj.motif[ui_motif];
    uint8_t y = (uint8_t)(4 + r);
    MotifNote *n = &m->n[r];
    char s[21];
    memset(s, ' ', 20); s[20] = 0;
    s[0] = (char)('1' + r);
    if (r < m->len) {
        int8_t rel = n->rel;
        s[2] = rel < 0 ? '-' : '+';
        fmt_num(s + 3, (uint16_t)(rel < 0 ? -rel : rel), 2);
        s[5] = ' ';
        fmt_note(s + 6, (uint8_t)(m->base + rel));
        s[9] = ' ';
        fmt_num(s + 10, n->dt, 3); s[13] = ' ';
        fmt_num(s + 15, n->dur, 3);
    } else { s[2] = '-'; s[3] = '-'; }
    put_strn(1, y, s, 19);
    put_tile(0, y, sub_cur == r ? T_CURSOR : 0);
    if (sub_cur == r && r < m->len) {
        uint8_t x = ncol == 0 ? 3 : (ncol == 1 ? 11 : 16);
        lb[0] = s[x - 1]; lb[1] = s[x]; lb[2] = s[x + 1]; lb[3] = 0;
        put_inv(x, y, lb, 3);
    }
}

static void draw_notes(void)
{
    uint8_t r;
    gfx_cls();
    ui_header("notes");
    put_low(1, 2, "length", 6);
    put_num(8, 2, proj.motif[ui_motif].len, 1);
    put_low(1, 3, "n  rel note wait len", 19);
    for (r = 0; r < MOTIF_MAX; r++) notes_row(r);
    put_low(1, 13, "wait/len in 1/15 s", 19);
    ui_help("@NOTES");
}

static void notes_keys(uint8_t keys)
{
    Motif *m = &proj.motif[ui_motif];
    uint8_t old = sub_cur;
    if (keys & K_B) { level = LV_MOTIF; draw_motif(); return; }
    if (keys & K_UP) { if (sub_cur == 0) sub_cur = 0xFF; else if (sub_cur != 0xFF) sub_cur--; }
    if (keys & K_DOWN) { if (sub_cur == 0xFF) sub_cur = 0; else if (sub_cur + 1 < MOTIF_MAX) sub_cur++; }
    if (keys & K_A) ncol = (uint8_t)((ncol + 1) % 3);
    if (keys & (K_LEFT | K_RIGHT)) {
        int8_t d = (keys & K_RIGHT) ? 1 : -1;
        if (sub_cur == 0xFF) {
            int8_t l = (int8_t)m->len + d;
            if (l < 0) l = 0;
            if (l > MOTIF_MAX) l = MOTIF_MAX;
            while (m->len < (uint8_t)l) {
                MotifNote *n = &m->n[m->len];
                n->rel = m->len ? m->n[m->len - 1].rel : 0;
                n->dt = m->len ? 12 : 0; n->dur = 10;
                m->len++;
            }
            m->len = (uint8_t)l;
            if (!m->base) m->base = 60;
            if (!m->tscale) m->tscale = 2;
            draw_notes();
            return;
        } else if (sub_cur < m->len) {
            MotifNote *n = &m->n[sub_cur];
            if (ncol == 0) { int8_t v = (int8_t)(n->rel + d); if (v >= -24 && v <= 24) n->rel = v; }
            else if (ncol == 1) { int16_t v = (int16_t)n->dt + d * (joy_accel > 8 ? 4 : 1); if (v >= 0 && v <= 255) n->dt = (uint8_t)v; }
            else { int16_t v = (int16_t)n->dur + d * (joy_accel > 8 ? 4 : 1); if (v >= 1 && v <= 255) n->dur = (uint8_t)v; }
            if (ncol == 0) engine_audition(m->inst, (uint8_t)(m->base + n->rel), 10);
        }
    }
    put_tile(0, 2, sub_cur == 0xFF ? T_CURSOR : 0);
    put_num(8, 2, m->len, 1);
    if (old != 0xFF && old < MOTIF_MAX) notes_row(old);
    if (sub_cur != 0xFF) notes_row(sub_cur);
}

void pg_form(uint8_t keys, uint8_t mode) BANKED
{
    if (mode == M_ENTER) { level = LV_TOP; naming = 0; ui_var[0] = eng.form.scene; ui_var[1] = eng.form.scene; ui_var[2] = 8; mode = M_DRAW; }
    if (mode == M_DRAW) {
        switch (level) {
        case LV_SCENES: draw_scenes(); break;
        case LV_SCENE:  draw_scene(); break;
        case LV_MOTIFS: draw_motifs(); break;
        case LV_MOTIF:  draw_motif(); break;
        case LV_NOTES:  draw_notes(); break;
        default:        draw_top(); break;
        }
        return;
    }
    if (mode == M_TICK) {
        if (level == LV_TOP && (ui_frame_cnt & 31) == 0) top_status();
        return;
    }
    switch (level) {
    case LV_TOP: {
        uint8_t r;
        if (keys & K_B) { ui_home(); return; }
        r = list_run(top_tab, N_TOP, 2, N_TOP, &cur, &top, keys);
        if (r == LI_ACTION) {
            if (cur == 4) { form_goto(ui_var[0], 0); ui_msg("SCENE CHANGE"); top_status(); }
            else if (cur == 5) {
                if (confirm != 5) { confirm = 5; ui_msg("A AGAIN: OVERWRITE"); }
                else { form_store_scene(ui_var[1]); confirm = 0xFF; ui_msg("SCENE STORED"); }
            } else if (cur == 6) { level = LV_SCENES; cur = 0; draw_scenes(); }
            else if (cur == 7) { level = LV_MOTIFS; cur = 0; draw_motifs(); }
        } else if (r) confirm = 0xFF;
        break; }
    case LV_SCENES: {
        uint8_t old = cur;
        if (keys & K_B) { level = LV_TOP; cur = 6; draw_top(); break; }
        if ((keys & K_UP) && cur > 0) cur--;
        if ((keys & K_DOWN) && cur < NUM_SCENES - 1) cur++;
        if (old != cur) { scenes_row(old); scenes_row(cur); }
        if (keys & K_A) { ui_scene = cur; level = LV_SCENE; sub_cur = 0; sub_top = 0; draw_scene(); }
        break; }
    case LV_SCENE: {
        uint8_t r;
        if (naming) {
            if (!text_edit(proj.scene[ui_scene].name, SCENE_NAME_LEN, 11, (uint8_t)(2 + sub_cur - sub_top), keys, &npos)) { naming = 0; draw_scene(); }
            return;
        }
        r = list_run(scene_tab, N_SCENE, 2, 13, &sub_cur, &sub_top, keys);
        if (r == LI_BACK) { level = LV_SCENES; cur = ui_scene; draw_scenes(); }
        else if (r == LI_ACTION && sub_cur == 0) {
            naming = 1; npos = 0;
            text_edit(proj.scene[ui_scene].name, SCENE_NAME_LEN, 11, (uint8_t)(2 + sub_cur - sub_top), 0, &npos);
            ui_help("@TEXTEDIT");
        } else if (r == LI_CHANGED && ui_scene == eng.form.scene) {
            /* edits to the running scene take effect on its live copy */
            memcpy(eng.form.live.s, proj.scene[ui_scene].s, SC_COUNT);
            form_eco_targets();
        }
        break; }
    case LV_MOTIFS: {
        uint8_t old = cur;
        if (keys & K_B) { level = LV_TOP; cur = 7; draw_top(); break; }
        if ((keys & K_UP) && cur > 0) cur--;
        if ((keys & K_DOWN) && cur < NUM_MOTIFS - 1) cur++;
        if (old != cur) { motifs_row(old); motifs_row(cur); }
        if (keys & K_A) { ui_motif = cur; level = LV_MOTIF; sub_cur = 0; draw_motif(); }
        break; }
    case LV_MOTIF: {
        Motif *m = &proj.motif[ui_motif];
        uint8_t oldsrc = m->source;
        uint8_t r = list_run(motif_tab, N_MOTIF, 2, N_MOTIF, &sub_cur, &top, keys);
        if (r == LI_BACK) { level = LV_MOTIFS; cur = ui_motif; draw_motifs(); break; }
        if (r == LI_CHANGED && sub_cur == 8) {
            if (oldsrc < NUM_AGENTS) agent_init(oldsrc);
            if (m->source < NUM_AGENTS) agent_init(m->source);
        }
        if (r == LI_ACTION) {
            switch (sub_cur) {
            case 0:
                if (!m->len) ui_msg("EMPTY MOTIF");
                else { engine_set_playing(eng.playing); motif_play(m, 9, 2); if (!eng.playing) ui_msg("PLAYS WHEN RUNNING"); }
                break;
            case 1: motif_mutate(m, &eng.rng); ui_msg("MUTATED"); break;
            case 4: motif_rhythm(m, 2); ui_msg("THINNED"); break;
            case 5: motif_rhythm(m, 3); ui_msg("FILLED"); break;
            case 10:
                if (motif_capture(ui_motif, ui_var[2] >= NUM_AGENTS ? 0xFF : ui_var[2], 6)) ui_msg("CAPTURED");
                else ui_msg("NOTHING TO CAPTURE");
                break;
            case 11: level = LV_NOTES; sub_cur = 0; ncol = 0; draw_notes(); return;
            case 12: motif_clear(m); ui_msg("CLEARED"); break;
            default: break;
            }
            draw_motif();
        }
        break; }
    default:
        notes_keys(keys);
        break;
    }
}
