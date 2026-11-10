/*
 * AETHER - by Synth Centric
 * ui_menu.c : the menu (SELECT), the song browser and FINE TUNE.
 *
 * Everything beyond the PLAY screen is reached from one plain-language
 * menu.  Each item says what it is for on the help panel.
 */
#pragma bank 255
#include <gb/gb.h>
#include <string.h>
#include "ui.h"
#include "engine.h"
#include "audio.h"

/* ------------------------------------------------------------------ menu */
#define N_MENU 7
static const char * const menu_name[N_MENU] = {
    "songs", "sounds", "players", "key", "sections", "fine tune", "save / load"
};
static const char * const menu_help[N_MENU][2] = {
    { "pick a piece to", "listen to" },
    { "shape the 8 sounds", "and draw wavetables" },
    { "the 8 musicians that", "write the notes" },
    { "key, scale and how", "consonant it is" },
    { "parts of the piece", "and how they change" },
    { "the 6 mood controls,", "tempo and volume" },
    { "keep your work in", "8 memory slots" },
};
static const uint8_t menu_page[N_MENU] = {
    PG_SONGS, PG_SYNTH, PG_AGENTS, PG_HARMONY, PG_FORM, PG_TUNE, PG_PROJECT
};
static uint8_t mcur;

static void menu_row(uint8_t i)
{
    uint8_t y = (uint8_t)(3 + i * 1 + (i ? 0 : 0));
    put_tile(1, y, mcur == i ? T_CURSOR : 0);
    put_low(3, y, menu_name[i], 12);
}

static void menu_draw(void)
{
    uint8_t i;
    gfx_cls();
    ui_header(0);
    for (i = 0; i < N_MENU; i++) menu_row(i);
    put_low(3, 11, "b / sel  back to", 16);
    put_low(3, 12, "the mood pad", 16);
    ui_help2(menu_help[mcur][0], menu_help[mcur][1]);
}

void pg_menu(uint8_t keys, uint8_t mode) BANKED
{
    uint8_t old = mcur;
    if (mode == M_ENTER || mode == M_DRAW) { menu_draw(); return; }
    if (mode == M_TICK) return;
    if ((keys & K_UP) && mcur > 0) mcur--;
    if ((keys & K_DOWN) && mcur < N_MENU - 1) mcur++;
    if (old != mcur) { menu_row(old); menu_row(mcur); ui_help2(menu_help[mcur][0], menu_help[mcur][1]); }
    if (keys & (K_A | K_RIGHT)) { ui_goto(menu_page[mcur]); return; }
    if (keys & K_B) ui_goto(PG_LIVE);
}

/* ----------------------------------------------------------------- songs */
static const char * const song_help[NUM_PRESETS][2] = {
    { "a gentle opening in", "five slow parts" },
    { "distant bells over", "a wide drone" },
    { "unstable tones and", "electric crackle" },
    { "soft chords that", "slowly change" },
    { "sparse notes, long", "silences, echoes" },
    { "bright repeating", "figures, mutating" },
    { "low pulses and", "rain-like rhythms" },
    { "small loops that", "drift in and out" },
    { "a melody remembered", "returning, fading" },
};
static uint8_t scur, sloaded = 0;

static void song_row(uint8_t i)
{
    char nm[PROJ_NAME_LEN + 1];
    uint8_t y = (uint8_t)(3 + i);
    preset_name(i, nm);
    put_tile(0, y, scur == i ? T_CURSOR : 0);
    put_tile(1, y, sloaded == i ? T_NOTE : 0);
    put_low(2, y, nm, PROJ_NAME_LEN);
}

static void songs_draw(void)
{
    uint8_t i;
    gfx_cls();
    ui_header(0);
    for (i = 0; i < NUM_PRESETS; i++) song_row(i);
    put_low(2, 13, "a: play this song", 18);
    ui_help2(song_help[scur][0], song_help[scur][1]);
}

void pg_songs(uint8_t keys, uint8_t mode) BANKED
{
    uint8_t old = scur;
    if (mode == M_ENTER || mode == M_DRAW) { songs_draw(); return; }
    if (mode == M_TICK) return;
    if ((keys & K_UP) && scur > 0) scur--;
    if ((keys & K_DOWN) && scur < NUM_PRESETS - 1) scur++;
    if (old != scur) { song_row(old); song_row(scur); ui_help2(song_help[scur][0], song_help[scur][1]); }
    if (keys & K_B) { ui_goto(PG_MENU); return; }
    if (keys & K_A) {
        engine_set_playing(0);
        preset_load(scur);
        sloaded = scur;
        engine_init();
        audio_set_master(proj.glob[GP_MVOL]);
        engine_set_playing(1);
        ui_goto(PG_LIVE);
    }
}

/* ------------------------------------------------------------- fine tune */
static const char * const tm_names[] = { "steady", "free" };
static const PDesc tune_tab[] = {
    { "DENSITY",   B_MACRO, MAC_DENSITY, 0, 15, F_BAR, 0, 0 },
    { "MOTION",    B_MACRO, MAC_MOTION, 0, 15, F_BAR, 0, 0 },
    { "SPACE",     B_MACRO, MAC_SPACE, 0, 15, F_BAR, 0, 0 },
    { "HARMONY",   B_MACRO, MAC_HARMONY, 0, 15, F_BAR, 0, 0 },
    { "EVOLUTION", B_MACRO, MAC_EVOLUTION, 0, 15, F_BAR, 0, 0 },
    { "TEXTURE",   B_MACRO, MAC_TEXTURE, 0, 15, F_BAR, 0, 0 },
    { "PULSE",     B_GLOB, GP_TMODE, 0, 1, F_NAME, 0, tm_names },
    { "TEMPO",     B_GLOB, GP_TEMPO, 30, 200, F_NUM, 0, 0 },
    { "VOLUME",    B_GLOB, GP_MVOL, 0, 7, F_BAR, 0, 0 },
};
#define N_TUNE (sizeof(tune_tab) / sizeof(PDesc))
static uint8_t tcur, ttop;
static const char * const tune_help[9][2] = {
    { "how many notes", "the players play" },
    { "how quickly they", "move and decide" },
    { "silence, echoes and", "stereo width" },
    { "how far harmony may", "stray from calm" },
    { "how fast ideas", "change over time" },
    { "noise texture and", "timbre movement" },
    { "steady: on a beat", "free: no beat" },
    { "speed of the beat", "(steady pulse only)" },
    { "master volume", "" },
};
static void tune_help_show(void) { ui_help2(tune_help[tcur][0], tune_help[tcur][1]); }

void pg_tune(uint8_t keys, uint8_t mode) BANKED
{
    uint8_t r;
    if (mode == M_ENTER || mode == M_DRAW) {
        gfx_cls();
        ui_header(0);
        list_draw(tune_tab, N_TUNE, 2, N_TUNE, tcur, 0);
        put_low(1, 12, "the mood pad moves", 19);
        put_low(1, 13, "the first six", 19);
        tune_help_show();
        return;
    }
    if (mode == M_TICK) {
        /* scenes glide the macros: keep the bars honest */
        if ((ui_frame_cnt & 15) == 0) {
            uint8_t i;
            for (i = 0; i < 6; i++) list_row(tune_tab, i, (uint8_t)(2 + i), i == tcur);
        }
        return;
    }
    r = list_run(tune_tab, N_TUNE, 2, N_TUNE, &tcur, &ttop, keys);
    if (r == LI_BACK) { ui_goto(PG_MENU); return; }
    if (r == LI_MOVED) tune_help_show();
    if (r == LI_CHANGED) {
        if (tcur < 6) eng.form.mac_tgt[tune_tab[tcur].off] = proj.macro[tune_tab[tcur].off];
        audio_set_master(proj.glob[GP_MVOL]);
    }
}
