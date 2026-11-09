/*
 * AETHER - by Synth Centric
 * audio_queue.c : lock-free single-producer / single-consumer event queue.
 * Lives in bank 0 so both the interrupt driver and the banked generator
 * can use it.  Portable (also used by the host tests).
 */
#include <string.h>
#include "audio.h"

NoteEvent aq[AQ_SIZE];
volatile uint8_t aq_head;
volatile uint8_t aq_tail;
uint8_t live_wave[NUM_WAVES][16];
volatile uint8_t live_wave_ver;
uint8_t wave_stage[2][16];

uint8_t aq_space(void) NONBANKED
{
    return (uint8_t)(AQ_MASK - ((aq_head - aq_tail) & AQ_MASK));
}

void aq_push(const NoteEvent *e) NONBANKED
{
    uint8_t h = aq_head;
    memcpy(&aq[h], e, sizeof(NoteEvent));
    aq_head = (uint8_t)((h + 1) & AQ_MASK);   /* publish after the copy */
}

void aq_clear(void) NONBANKED
{
    AE_CRITICAL_BEGIN
        aq_tail = aq_head;
    AE_CRITICAL_END
}
