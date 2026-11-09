/*
 * AETHER - by Synth Centric
 * simulate.c : deterministic host simulation of long performances.
 * Runs the real generative core frame by frame (59.73 Hz) and consumes the
 * event queue exactly like the VBlank driver, then reports statistics.
 * This is a SIMULATION of the engine logic, not an emulator run.
 *   usage: simulate <preset 0-8> <minutes> [seed] [-v]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "engine.h"
#include "audio.h"

int main(int argc, char **argv)
{
    int preset = argc > 1 ? atoi(argv[1]) : 0;
    long minutes = argc > 2 ? atol(argv[2]) : 10;
    int verbose = argc > 4 && !strcmp(argv[4], "-v");
    long frames = minutes * 60 * 5973 / 100, f;
    long per_agent[10] = {0}, per_ch[4] = {0}, busy[4] = {0};
    long win_notes = 0;
    uint16_t until[4] = {0};
    int lo = 255, hi = 0, last_scene = -1;
    unsigned emin[4] = {255,255,255,255}, emax[4] = {0};
    preset_load(preset);
    if (argc > 3 && strcmp(argv[3], "-")) { uint16_t s = (uint16_t)atoi(argv[3]); proj.seed_lo = s & 255; proj.seed_hi = s >> 8; }
    audio_init(); engine_init(); engine_set_playing(1);
    printf("preset %d '%.18s' %ld min\n", preset, proj.name, minutes);
    for (f = 0; f < frames; f++) {
        int c;
        audio_now++;
        engine_service(audio_now);
        while (aq_tail != aq_head) {
            NoteEvent *e = &aq[aq_tail];
            if ((int16_t)(e->time - audio_now) > 0) break;
            per_ch[e->ch]++;
            until[e->ch] = (uint16_t)(audio_now + e->dur);
            if (e->ch != CH_NOISE) { if (e->note < lo) lo = e->note; if (e->note > hi) hi = e->note; }
            win_notes++;
            aq_tail = (aq_tail + 1) & AQ_MASK;
        }
        for (c = 0; c < 4; c++) if ((int16_t)(until[c] - audio_now) > 0) busy[c]++;
        for (c = 0; c < 4; c++) { unsigned v = eng.eco.v[c] >> 8; if (v < emin[c]) emin[c] = v; if (v > emax[c]) emax[c] = v; }
        if (eng.form.scene != last_scene) {
            if (verbose || last_scene < 0 || minutes <= 120) printf("  %6.1fs scene -> S%d %.8s\n", f / 59.73, eng.form.scene + 1, proj.scene[eng.form.scene].name);
            last_scene = eng.form.scene;
        }
        if (verbose && f % (59 * 60) == 0)
            printf("  t=%3ldm notes/min=%4ld E%3u T%3u S%3u M%3u tide%3u ero%3u chord%u root%u\n", f / 3584, win_notes,
                   eng.eco.v[0] >> 8, eng.eco.v[1] >> 8, eng.eco.v[2] >> 8, eng.eco.v[3] >> 8, eng.tide, eng.erosion, eng.hs.degree, eng.hs.root), win_notes = 0;
        if (!eng.playing) { printf("  composition ended at %.1fs\n", f / 59.73); break; }
    }
    {
        int i;
        long tot = 0;
        for (i = 0; i < 4; i++) tot += per_ch[i];
        for (i = 0; i < HIST_N; i++) {}
        printf("events %ld (%.1f/min)  gen=%u dropped=%u stolen=%u eroded=%u qfull=%u\n", tot, tot / (double)minutes,
               eng.st_events, eng.st_dropped, eng.st_stolen, eng.st_eroded, eng.st_qfull);
        printf("channel events: P1 %ld P2 %ld W %ld N %ld | occupancy %% %ld %ld %ld %ld\n", per_ch[0], per_ch[1], per_ch[2], per_ch[3],
               busy[0] * 100 / f, busy[1] * 100 / f, busy[2] * 100 / f, busy[3] * 100 / f);
        printf("pitch range %d..%d  eco ranges E%u-%u T%u-%u S%u-%u M%u-%u  transitions %u harmonic changes %u forever %u\n", lo, hi,
               emin[0], emax[0], emin[1], emax[1], emin[2], emax[2], emin[3], emax[3], eng.form.transitions, eng.hs.changes, eng.form.forever_gen);
        (void)per_agent;
        printf("per source (played/dropped): ");
        for (i = 0; i < 10; i++) printf("%s %u/%u  ", (const char*[]){"ORB","BLM","DRF","SCT","TID","ECH","ERO","MEM","TEX","USR"}[i], eng.st_ag_ev[i], eng.st_ag_drop[i]);
        printf("\n");
    }
    return 0;
}
