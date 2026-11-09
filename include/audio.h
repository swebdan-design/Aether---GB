/*
 * AETHER - by Synth Centric
 * audio.h : event queue between the generator (main loop) and the
 * frame-rate voice driver (VBlank interrupt).
 *
 * Single producer (main loop) / single consumer (interrupt) ring buffer.
 * Indices are 8-bit, so their updates are atomic on the SM83.
 */
#ifndef AETHER_AUDIO_H
#define AETHER_AUDIO_H

#include "aether.h"

#define AQ_SIZE 32               /* power of two */
#define AQ_MASK (AQ_SIZE - 1)

#define NEF_OFF    0x01          /* release the channel                    */
#define NEF_LEGATO 0x02          /* change pitch without retrigger         */

typedef struct {
    uint16_t time;     /* frame stamp at which the event plays       */
    uint16_t dur;      /* gate length in frames                      */
    uint8_t  ch;       /* hardware channel 0-3                       */
    uint8_t  note;     /* MIDI note (noise: index 0-23)              */
    uint8_t  inst;     /* instrument slot                            */
    uint8_t  vol;      /* initial volume 0-15                        */
    uint8_t  pan;      /* bit0 = left, bit1 = right                  */
    uint8_t  aux;      /* wave: morph amount 0-8 | pulse: duty+1     */
    int8_t   detune;   /* register units                             */
    uint8_t  flags;
} NoteEvent;

extern NoteEvent aq[AQ_SIZE];
extern volatile uint8_t aq_head;  /* written by producer */
extern volatile uint8_t aq_tail;  /* written by consumer */

uint8_t aq_space(void) NONBANKED;
void    aq_push(const NoteEvent *e) NONBANKED;
void    aq_clear(void) NONBANKED;

/* live (runtime) wavetables used by the driver; edits and evolution
 * operate on these copies so the project's stored waves stay intact */
extern uint8_t live_wave[NUM_WAVES][16];
extern volatile uint8_t live_wave_ver;   /* bumped on every change */

/* --- hardware driver (Game Boy only) ------------------------------ */
extern volatile uint16_t audio_now;       /* frame counter            */
extern volatile uint8_t  ch_meter[NUM_CH];/* 0-15 approx level (UI)   */
extern volatile uint8_t  isr_lines;       /* profiling: scanlines used */
uint16_t audio_time(void) NONBANKED;
void     audio_init(void) NONBANKED;
void     audio_isr(void) NONBANKED;
void     audio_all_off(void) NONBANKED;
void     audio_set_master(uint8_t vol) NONBANKED;

/* frequency register value for MIDI note on pulse channels (36..107) */
uint16_t note_freq(uint8_t note) NONBANKED;

#endif
