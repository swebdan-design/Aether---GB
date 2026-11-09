/*
 * AETHER - by Synth Centric
 * generative ambient instrument for the original Nintendo Game Boy (DMG-01)
 *
 * main.c : boot, interrupt setup and the cooperative main loop.
 *
 *   VBlank interrupt  -> audio_isr(): plays due events, per-frame voice
 *                        modulation (fixed bank 0, never blocked by the UI)
 *   main loop         -> engine_service(): generates musical decisions up to
 *                        LOOKAHEAD frames ahead into the event queue
 *                     -> ui_frame(): input + incremental screen updates
 *                     -> engine_service() again (refill after slow redraws)
 *                     -> wait for the next frame
 */
#include <gb/gb.h>
#include "aether.h"
#include "engine.h"
#include "audio.h"
#include "ui.h"
#include "gfx.h"

Project proj;
uint8_t prof_gen_lines, prof_ui_lines, prof_gen_max, prof_ui_max;

static uint8_t lines_since(uint8_t ly0, uint16_t f0)
{
    uint16_t frames = (uint16_t)(audio_time() - f0);
    uint8_t ly1 = LY_REG;
    int16_t l = (int16_t)frames * 154 + (int16_t)ly1 - (int16_t)ly0;
    if (l < 0) l = 0;
    if (l > 255) l = 255;
    return (uint8_t)l;
}

void main(void)
{
    uint8_t ly0;
    uint16_t f0;

    DISPLAY_OFF;
    disable_interrupts();
    audio_init();
    gfx_init();
    preset_load(0);                 /* first-run composition: FIRST LIGHT */
    engine_init();
    audio_set_master(proj.glob[GP_MVOL]);
    ui_init();

    add_VBL(audio_isr);
    add_VBL(input_isr);
    set_interrupts(VBL_IFLAG);
    enable_interrupts();

    ui_title();

    while (1) {
        ly0 = LY_REG; f0 = audio_time();
        engine_service(audio_time());
        prof_gen_lines = lines_since(ly0, f0);
        if (prof_gen_lines > prof_gen_max) prof_gen_max = prof_gen_lines;

        ly0 = LY_REG; f0 = audio_time();
        ui_frame();
        prof_ui_lines = lines_since(ly0, f0);
        if (prof_ui_lines > prof_ui_max) prof_ui_max = prof_ui_lines;

        engine_service(audio_time());
        wait_vbl_done();
    }
}
