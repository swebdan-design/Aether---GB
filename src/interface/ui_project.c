/*
 * AETHER - by Synth Centric
 * ui_project.c : PROJECT page - name, reproducible seed, restart, battery
 * SRAM slots (save / load with confirmation), factory compositions,
 * performance statistics and credits.
 */
#pragma bank 255
#include <gb/gb.h>
#include <string.h>
#include "ui.h"
#include "engine.h"
#include "audio.h"
#include "storage.h"

#define LV_TOP 0
#define LV_STATS 1
#define LV_CREDITS 2

#define R_NAME 0
#define R_SEED 1
#define R_RESTART 2
#define R_SLOT 3
#define R_SAVE 4
#define R_LOAD 5
#define R_ERASE 6
#define R_FACTORY 7
#define R_STATS 8
#define R_CREDITS 9

static uint8_t level, cur, top, naming, npos, confirm = 0xFF;

static const char * const a_rename[] = { "RENAME" };
static const char * const a_seed[] = { "" };
static const char * const a_restart[] = { "a: go" };
static const char * const a_save[] = { "a: save" };
static const char * const a_load[] = { "a: load" };
static const char * const a_erase[] = { "a: erase" };
static const char * const a_open[] = { "a: open" };
static const char * const preset_list[NUM_PRESETS] = {
    "FIRST LT", "PALE TX", "STATIC M", "DISSOLV", "EMPTY RM", "GLASS ORB", "NIGHT CUR", "SOFT MACH", "AFTERIMG"
};

static const PDesc prj_tab[] = {
    { "NAME",      B_NONE, 0, 0, 0, F_ACTION, 0, a_rename },
    { "SEED",      B_NONE, 0, 0, 0, F_ACTION, 0, a_seed },
    { "RESTART",   B_NONE, 0, 0, 0, F_ACTION, 0, a_restart },
    { "SLOT",      B_UI, 3, 0, 7, F_NUM, -1, 0 },
    { "SAVE",      B_NONE, 0, 0, 0, F_ACTION, 0, a_save },
    { "LOAD",      B_NONE, 0, 0, 0, F_ACTION, 0, a_load },
    { "ERASE",     B_NONE, 0, 0, 0, F_ACTION, 0, a_erase },
    { "FACTORY",   B_UI, 4, 0, NUM_PRESETS - 1, F_NAME, 0, preset_list },
    { "STATS",     B_NONE, 0, 0, 0, F_ACTION, 0, a_open },
    { "CREDITS",   B_NONE, 0, 0, 0, F_ACTION, 0, a_open },
};
#define N_PRJ (sizeof(prj_tab) / sizeof(PDesc))

static uint16_t seed_get(void) { return (uint16_t)(proj.seed_lo | ((uint16_t)proj.seed_hi << 8)); }
static void seed_set(uint16_t s) { proj.seed_lo = (uint8_t)s; proj.seed_hi = (uint8_t)(s >> 8); }

static void seed_row(void)
{
    put_num(11, (uint8_t)(2 + R_SEED), seed_get(), 5);
    put_strn(16, (uint8_t)(2 + R_SEED), "", 3);
}

static uint8_t slot_state;    /* cached status of the selected slot */

static void slot_info(void)
{
    static const char * const st[4] = { "EMPTY", "OK", "CORRUPT", "BACKUP" };
    char nm[PROJ_NAME_LEN + 1];
    uint8_t s = ui_var[3] & 7;
    slot_state = slot_status(s);
    put_low(1, 13, "slot", 4);
    put_num(6, 13, (uint16_t)(s + 1), 1);
    put_low(8, 13, st[slot_state & 3], 8);
    if (slot_state == SLOT_OK || slot_state == SLOT_BACKUP) {
        slot_name(s, nm);
        put_low(1, 14, nm, PROJ_NAME_LEN);
    } else put_strn(1, 14, "", 19);
}

static void draw_top(void)
{
    gfx_cls();
    ui_header(0);
    put_strn(0, 12, "", 20);
    put_low(1, 12, proj.name, PROJ_NAME_LEN);
    list_draw(prj_tab, N_PRJ, 2, N_PRJ, cur, 0);
    seed_row();
    slot_info();
}

static void stats(void)
{
    static const char * const lab[10] = {
        "EVENTS", "DROPPED", "STOLEN", "ERODED", "QUEUE FULL", "MAX BEHIND", "GEN LINES", "UI LINES", "ISR LINES", "HARM CHG"
    };
    uint16_t v[10];
    uint8_t i;
    v[0] = eng.st_events; v[1] = eng.st_dropped; v[2] = eng.st_stolen; v[3] = eng.st_eroded;
    v[4] = eng.st_qfull; v[5] = eng.st_maxbehind; v[6] = prof_gen_max; v[7] = prof_ui_max;
    v[8] = isr_lines; v[9] = eng.hs.changes;
    for (i = 0; i < 10; i++) {
        put_low(1, (uint8_t)(2 + i), lab[i], 11);
        put_num(13, (uint8_t)(2 + i), v[i], 6);
    }
    put_low(1, 12, "time", 4);
    fmt_time(lb, eng.form.total_s, eng.form.hours);
    put_strn(13, 12, lb, 7);
    put_low(1, 13, "scene changes", 13);
    put_num(15, 13, eng.form.transitions, 4);
}

static void draw_stats(void)
{
    gfx_cls();
    ui_header("stats");
    stats();
    ui_help2("lines: of 154/frame", "a: reset  b: back");
}

static void draw_credits(void)
{
    uint8_t i, t[16];
    gfx_cls();
    gfx_title_tiles();
    for (i = 0; i < 16; i++) t[i] = (uint8_t)(T_DYN + i);
    put_tiles(2, 1, 16, t);
    for (i = 0; i < 16; i++) t[i] = (uint8_t)(T_DYN + 16 + i);
    put_tiles(2, 2, 16, t);
    for (i = 0; i < 16; i++) t[i] = (uint8_t)(T_DYN + 32 + i);
    put_tiles(2, 3, 16, t);
    put_str(2, 5, "by Synth Centric");
    put_str(1, 6, "generative ambient");
    put_str(5, 7, "instrument");
    put_str(1, 9, "concept, sound, code");
    put_str(4, 10, "Synth Centric");
    put_str(1, 12, "for the original");
    put_str(1, 13, "Game Boy (DMG-01)");
    put_str(1, 15, "v" AETHER_VERSION_STR " GBDK-2020");
    ui_help("@CREDITS");
}

static void reload_engine(void)
{
    uint8_t was = eng.playing;
    engine_set_playing(0);
    engine_init();
    audio_set_master(proj.glob[GP_MVOL]);
    if (was) engine_set_playing(1);
}

static uint8_t confirm_ok(uint8_t row, const char *msg)
{
    if (confirm == row) { confirm = 0xFF; return 1; }
    confirm = row;
    ui_msg(msg);
    return 0;
}

void pg_project(uint8_t keys, uint8_t mode) BANKED
{
    if (mode == M_ENTER) { level = LV_TOP; naming = 0; confirm = 0xFF; mode = M_DRAW; }
    if (mode == M_DRAW) {
        if (level == LV_STATS) draw_stats();
        else if (level == LV_CREDITS) draw_credits();
        else draw_top();
        return;
    }
    if (mode == M_TICK) {
        if (level == LV_STATS && (ui_frame_cnt & 15) == 0) stats();
        return;
    }
    if (level != LV_TOP) {
        if (keys & K_B) { level = LV_TOP; draw_top(); }
        else if (level == LV_STATS && (keys & K_A)) {
            prof_gen_max = 0; prof_ui_max = 0; isr_lines = 0; eng.st_maxbehind = 0;
            stats();
        }
        return;
    }
    if (naming) {
        if (!text_edit(proj.name, PROJ_NAME_LEN, 1, 12, keys, &npos)) { naming = 0; draw_top(); }
        return;
    }
    if (cur == R_SEED && (keys & (K_LEFT | K_RIGHT))) {
        uint16_t s = seed_get();
        uint16_t step = joy_accel > 30 ? 256 : (joy_accel > 10 ? 16 : 1);
        if (keys & K_RIGHT) s += step; else s -= step;
        seed_set(s);
        seed_row();
        return;
    }
    {
        uint8_t old = cur;
        uint8_t r;
        if (keys & K_B) { ui_home(); return; }
        r = list_run(prj_tab, N_PRJ, 2, N_PRJ, &cur, &top, keys);
        if (cur != old) { confirm = 0xFF; seed_row(); }
        if (r == LI_CHANGED && cur == R_SLOT) slot_info();
        if (r != LI_ACTION) return;
    }
    switch (cur) {
    case R_NAME:
        naming = 1; npos = 0;
        text_edit(proj.name, PROJ_NAME_LEN, 1, 12, 0, &npos);
        ui_help("@TEXTEDIT");
        break;
    case R_SEED:
        seed_set((uint16_t)(DIV_REG | ((uint16_t)ui_frame_cnt << 8)) ^ (uint16_t)(audio_time() * 31));
        seed_row();
        ui_msg("NEW SEED: RESTART");
        break;
    case R_RESTART:
        reload_engine();
        ui_msg("RESTARTED FROM SEED");
        break;
    case R_SAVE:
        if (confirm_ok(R_SAVE, "A AGAIN: SAVE SLOT")) {
            ui_msg(save_slot(ui_var[3] & 7) ? "SAVED + VERIFIED" : "SAVE FAILED");
            slot_info();
        }
        break;
    case R_LOAD:
        if (slot_state == SLOT_EMPTY || slot_state == SLOT_CORRUPT) {
            ui_msg("NO VALID PROJECT");
            break;
        }
        if (confirm_ok(R_LOAD, "A AGAIN: LOAD SLOT")) {
            if (load_slot(ui_var[3] & 7)) { reload_engine(); ui_msg("LOADED"); draw_top(); }
            else ui_msg("LOAD FAILED");
        }
        break;
    case R_ERASE:
        if (confirm_ok(R_ERASE, "A AGAIN: ERASE SLOT")) { slot_erase(ui_var[3] & 7); ui_msg("ERASED"); slot_info(); }
        break;
    case R_FACTORY:
        if (confirm_ok(R_FACTORY, "A AGAIN: LOAD PRESET")) {
            preset_load(ui_var[4]);
            reload_engine();
            ui_msg("COMPOSITION LOADED");
            draw_top();
        }
        break;
    case R_STATS: level = LV_STATS; draw_stats(); break;
    case R_CREDITS: level = LV_CREDITS; draw_credits(); break;
    default: break;
    }
}
