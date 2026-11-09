/*
 * AETHER - by Synth Centric
 * audio_hw.c : frame-rate voice driver for the DMG programmable sound
 * generator.  Runs from the VBlank interrupt (~59.73 Hz) in fixed bank 0.
 *
 * Responsibilities (deliberately small and predictable):
 *   - pop due events from the queue and trigger hardware channels
 *   - gate countdown and click-minimised note release
 *   - per-frame software modulation: pitch envelope, arpeggio, vibrato,
 *     pulse duty cycling, wave level fade, noise timbre drift
 *   - per-note stereo routing through NR51
 *   - level estimation for the UI meters
 * No musical decisions are taken here; the generator decides everything.
 */
#include <gb/gb.h>
#include <gb/hardware.h>
#include "audio.h"

extern const uint16_t freq_table[72];
extern const uint8_t  noise_table[24];

volatile uint16_t audio_now;
volatile uint8_t  ch_meter[NUM_CH];
volatile uint8_t  isr_lines;

typedef struct {
    uint8_t  active;
    uint16_t left;        /* gate frames remaining           */
    uint16_t base;        /* target frequency register       */
    uint16_t cur;         /* current (pitch envelope)        */
    uint8_t  note;
    uint8_t  inst;
    int8_t   detune;
    uint8_t  age;
    uint8_t  vib_phase;
    uint8_t  arp_pos, arp_cnt;
    uint8_t  duty, duty_cnt;
    uint8_t  fade_cnt, level, level_tgt;
    uint8_t  drift_cnt, nidx, nbase;
    uint8_t  env, env_cnt;
    const uint8_t *p;     /* instrument parameters (cached at trigger) */
    uint16_t vspan;       /* vibrato span for the current pitch       */
} ChanRt;

static ChanRt rt[NUM_CH];
static uint8_t nr51_shadow;
static uint8_t wave_key, wave_ver;
static uint8_t isr_rng = 0x5A;

static const uint8_t penv_shift[16] = { 5, 5, 5, 4, 4, 4, 3, 3, 3, 2, 2, 2, 1, 1, 1, 0 };

static const int8_t arp_tab[8][4] = {
    {0, 0, 0, 0},  {0, 12, 0, 12}, {0, 7, 12, 7}, {0, 4, 7, 12},
    {0, 3, 7, 10}, {0, 5, 7, 12},  {0, 7, 14, 19}, {0, 12, 24, 12}
};

uint16_t note_freq(uint8_t note) NONBANKED
{
    if (note < 36) note = 36 + (note % 12);
    if (note > 107) note = 96 + (note % 12);
    return freq_table[note - 36];
}

uint16_t audio_time(void) NONBANKED
{
    uint16_t t;
    __critical { t = audio_now; }
    return t;
}

static uint16_t clamp_f(int16_t f)
{
    if (f < 0) return 0;
    if (f > 2047) return 2047;
    return (uint16_t)f;
}

static void write_freq(uint8_t ch, uint16_t f)
{
    if (ch == CH_PULSE1) { NR13_REG = (uint8_t)f; NR14_REG = (uint8_t)(f >> 8); }
    else if (ch == CH_PULSE2) { NR23_REG = (uint8_t)f; NR24_REG = (uint8_t)(f >> 8); }
    else { NR33_REG = (uint8_t)f; NR34_REG = (uint8_t)(f >> 8); }
}

static uint8_t wave_level_code(uint8_t lvl)
{
    /* lvl 0 mute, 1 quiet (25%), 2 medium (50%), 3 full (100%) */
    if (lvl == 0) return 0x00;
    if (lvl == 1) return 0x60;
    if (lvl == 2) return 0x40;
    return 0x20;
}

static void note_off(uint8_t ch)
{
    ChanRt *c = &rt[ch];
    c->active = 0;
    c->env = 0;
    /* keep the DAC powered (envelope 0, "increase" bit set) to avoid the
     * DC step that a DAC power-down produces; retrigger silently. */
    switch (ch) {
    case CH_PULSE1: NR12_REG = 0x08; NR14_REG = 0x80; break;
    case CH_PULSE2: NR22_REG = 0x08; NR24_REG = 0x80; break;
    case CH_WAVE:   NR32_REG = 0x00; break;
    default:        NR42_REG = 0x08; NR44_REG = 0x80; break;
    }
}

static void load_wave(const uint8_t *w)
{
    uint8_t i;
    NR30_REG = 0x00;                 /* channel off: wave RAM is writable */
    for (i = 0; i < 16; i++) AUD3WAVE[i] = w[i];
    NR30_REG = 0x80;
}

static void play_event(NoteEvent *e)
{
    uint8_t ch = e->ch;
    ChanRt *c = &rt[ch];
    const uint8_t *p = proj.inst[e->inst & 7].p;
    uint8_t legato, bit;
    uint16_t f;

    if (e->flags & NEF_OFF) { note_off(ch); return; }

    legato = (e->flags & NEF_LEGATO) && c->active;
    c->inst = e->inst & 7;
    c->note = e->note;
    c->detune = e->detune;
    c->left = e->dur ? e->dur : 1;
    c->age = 0;
    c->arp_pos = 0; c->arp_cnt = 0;
    c->vib_phase = 0;
    c->duty_cnt = 0;
    c->fade_cnt = 0;
    c->drift_cnt = 0;
    c->env_cnt = 0;
    c->p = p;
    c->env = p[IP_ENVDIR] ? 0 : e->vol;
    if (p[IP_ENVDIR] && p[IP_ENVPACE] == 0) c->env = e->vol;

    if (ch <= CH_PULSE2) {
        f = clamp_f((int16_t)note_freq(e->note) + e->detune);
        c->base = f;
        if (p[IP_PENV] != 12 && !legato) {
            int8_t n = (int8_t)e->note + (int8_t)p[IP_PENV] - 12;
            c->cur = clamp_f((int16_t)note_freq((uint8_t)n) + e->detune);
        } else c->cur = f;
        c->duty = e->aux ? (uint8_t)((e->aux - 1) & 3) : (p[IP_SHAPE] & 3);
        if (ch == CH_PULSE1) {
            if (!legato)
                NR10_REG = (uint8_t)(((p[IP_SWPACE] & 7) << 4) | ((p[IP_SWDIR] & 1) << 3) | (p[IP_SWSTEP] & 7));
            NR11_REG = (uint8_t)(c->duty << 6);
            if (!legato) {
                NR12_REG = (uint8_t)((e->vol << 4) | ((p[IP_ENVDIR] & 1) << 3) | (p[IP_ENVPACE] & 7));
                NR13_REG = (uint8_t)c->cur;
                NR14_REG = (uint8_t)(0x80 | (c->cur >> 8));
            } else write_freq(ch, c->cur);
        } else {
            NR21_REG = (uint8_t)(c->duty << 6);
            if (!legato) {
                NR22_REG = (uint8_t)((e->vol << 4) | ((p[IP_ENVDIR] & 1) << 3) | (p[IP_ENVPACE] & 7));
                NR23_REG = (uint8_t)c->cur;
                NR24_REG = (uint8_t)(0x80 | (c->cur >> 8));
            } else write_freq(ch, c->cur);
        }
    } else if (ch == CH_WAVE) {
        uint8_t key = (uint8_t)(p[IP_WSLOT] & 3);
        uint8_t lvl = e->vol >= 11 ? 3 : (e->vol >= 6 ? 2 : (e->vol ? 1 : 0));
        if (p[IP_VOL] && p[IP_VOL] < lvl) lvl = p[IP_VOL];
        f = clamp_f((int16_t)note_freq((uint8_t)(e->note + 12)) + e->detune);
        c->base = f;
        if (p[IP_PENV] != 12 && !legato) {
            int8_t n = (int8_t)e->note + 12 + (int8_t)p[IP_PENV] - 12;
            c->cur = clamp_f((int16_t)note_freq((uint8_t)n) + e->detune);
        } else c->cur = f;
        c->level_tgt = lvl;
        if (!legato) {
            if (e->aux & AUX_STAGED) {
                load_wave(wave_stage[e->aux & 1]);
                wave_key = 0xFF;
            } else if (key != wave_key || live_wave_ver != wave_ver) {
                load_wave(live_wave[p[IP_WSLOT] & 3]);
                wave_key = key; wave_ver = live_wave_ver;
            } else {
                /* power-cycle the channel before retriggering: avoids the
                 * DMG wave-RAM corruption on retrigger-while-playing */
                NR30_REG = 0x00; NR30_REG = 0x80;
            }
            c->level = (p[IP_ENVDIR] && p[IP_ENVPACE]) ? 1 : lvl;
            NR32_REG = wave_level_code(c->level);
            NR33_REG = (uint8_t)c->cur;
            NR34_REG = (uint8_t)(0x80 | (c->cur >> 8));
        } else {
            write_freq(ch, c->cur);
        }
        c->env = (uint8_t)(c->level * 5);
    } else {
        c->nidx = e->note > 23 ? 23 : e->note;
        c->nbase = c->nidx;
        NR42_REG = (uint8_t)((e->vol << 4) | ((p[IP_ENVDIR] & 1) << 3) | (p[IP_ENVPACE] & 7));
        NR43_REG = (uint8_t)(noise_table[c->nidx] | ((p[IP_NWIDTH] & 1) << 3));
        NR44_REG = 0x80;
    }

    c->vspan = (uint16_t)(((2048 - c->base) >> 6) * p[IP_VIBD]);

    /* discrete stereo placement */
    bit = (uint8_t)(1 << ch);
    nr51_shadow &= (uint8_t)~(bit | (bit << 4));
    if (e->pan & 1) nr51_shadow |= (uint8_t)(bit << 4);
    if (e->pan & 2) nr51_shadow |= bit;
    if (!(e->pan & 3)) nr51_shadow |= (uint8_t)(bit | (bit << 4));
    NR51_REG = nr51_shadow;
    c->active = 1;
}

static void chan_update(uint8_t ch)
{
    ChanRt *c = &rt[ch];
    const uint8_t *p;
    uint16_t f;

    if (!c->active) { ch_meter[ch] = 0; return; }
    p = c->p;
    if (c->age != 255) c->age++;

    if (--c->left == 0) { note_off(ch); ch_meter[ch] = 0; return; }

    /* envelope estimate for the meters (hardware env clock is 64 Hz) */
    if (ch != CH_WAVE && p[IP_ENVPACE]) {
        if (++c->env_cnt >= p[IP_ENVPACE]) {
            c->env_cnt = 0;
            if (p[IP_ENVDIR]) { if (c->env < 15) c->env++; }
            else if (c->env) c->env--;
        }
    }
    ch_meter[ch] = c->env;

    if (ch == CH_NOISE) {
        if (p[IP_NDRIFTD]) {
            if (++c->drift_cnt >= (uint8_t)((16 - (p[IP_NDRIFTR] & 15)) << 1)) {
                int8_t n;
                c->drift_cnt = 0;
                isr_rng ^= (uint8_t)(isr_rng << 3); isr_rng ^= (uint8_t)(isr_rng >> 5); isr_rng ^= (uint8_t)(isr_rng << 1);
                n = (int8_t)c->nidx + ((isr_rng & 1) ? 1 : -1);
                if (n < (int8_t)c->nbase - (int8_t)p[IP_NDRIFTD]) n = (int8_t)c->nbase - (int8_t)p[IP_NDRIFTD];
                if (n > (int8_t)c->nbase + (int8_t)p[IP_NDRIFTD]) n = (int8_t)c->nbase + (int8_t)p[IP_NDRIFTD];
                if (n < 0) n = 0;
                if (n > 23) n = 23;
                c->nidx = (uint8_t)n;
                NR43_REG = (uint8_t)(noise_table[c->nidx] | ((p[IP_NWIDTH] & 1) << 3));
            }
        }
        return;
    }

    if (ch == CH_WAVE && p[IP_ENVPACE]) {
        if (++c->fade_cnt >= (uint8_t)(p[IP_ENVPACE] << 3)) {
            c->fade_cnt = 0;
            if (p[IP_ENVDIR]) { if (c->level < c->level_tgt) c->level++; }
            else if (c->level) c->level--;
            NR32_REG = wave_level_code(c->level);
            c->env = (uint8_t)(c->level * 5);
            ch_meter[ch] = c->env;
        }
    }

    /* arpeggio (replaces base pitch) */
    if (p[IP_ARP] & 7) {
        if (++c->arp_cnt >= (p[IP_ARPSPD] ? p[IP_ARPSPD] : 1)) {
            uint8_t ofs = (uint8_t)arp_tab[p[IP_ARP] & 7][c->arp_pos];
            c->arp_cnt = 0;
            c->arp_pos = (c->arp_pos + 1) & 3;
            c->base = clamp_f((int16_t)note_freq((uint8_t)(c->note + ofs + (ch == CH_WAVE ? 12 : 0))) + c->detune);
            c->cur = c->base;
        }
    }

    /* pitch envelope: exponential approach toward base */
    if (c->cur != c->base) {
        int16_t d = (int16_t)c->base - (int16_t)c->cur;
        uint8_t sh = penv_shift[p[IP_PENVSPD] & 15];
        int16_t step = d >> sh;
        if (step == 0) step = d > 0 ? 1 : -1;
        c->cur = (uint16_t)((int16_t)c->cur + step);
    }
    f = c->cur;

    /* vibrato: depth proportional to period so it is uniform in cents */
    if (p[IP_VIBD] && c->age > (uint8_t)(p[IP_VIBDLY] << 3)) {
        int8_t tri;
        int16_t span;
        c->vib_phase += (uint8_t)((p[IP_VIBR] + 1) << 1);
        tri = (int8_t)(c->vib_phase & 0x7F);
        if (tri > 63) tri = 127 - tri;          /* 0..63..0 */
        tri -= 32;                              /* -32..31  */
        span = (int16_t)c->vspan;
        f = clamp_f((int16_t)f + (int16_t)((span * tri) >> 8));
    }
    if (ch != CH_WAVE || c->level) write_freq(ch, f);

    /* pulse width cycling */
    if (ch != CH_WAVE && p[IP_MOD]) {
        if (++c->duty_cnt >= (uint8_t)((16 - (p[IP_MOD] & 15)) << 1)) {
            c->duty_cnt = 0;
            c->duty = (c->duty + 1) & 3;
            if (c->duty == 3) c->duty = 0;      /* 12.5/25/50% only: 75% sounds like 25% */
            if (ch == CH_PULSE1) NR11_REG = (uint8_t)(c->duty << 6);
            else NR21_REG = (uint8_t)(c->duty << 6);
        }
    }
}

void audio_isr(void) NONBANKED
{
    uint8_t ly0 = LY_REG, ly1;
    NoteEvent *e;
    audio_now++;
    while (aq_tail != aq_head) {
        e = &aq[aq_tail];
        if ((int16_t)(e->time - audio_now) > 0) break;
        play_event(e);
        aq_tail = (uint8_t)((aq_tail + 1) & AQ_MASK);
    }
    chan_update(0);
    chan_update(1);
    chan_update(2);
    chan_update(3);
    ly1 = LY_REG;
    ly1 = (uint8_t)(ly1 >= ly0 ? ly1 - ly0 : ly1 + 154 - ly0);
    if (ly1 > isr_lines) isr_lines = ly1;
}

void audio_all_off(void) NONBANKED
{
    uint8_t i;
    __critical {
        for (i = 0; i < NUM_CH; i++) note_off(i);
        aq_tail = aq_head;
    }
}

void audio_set_master(uint8_t vol) NONBANKED
{
    vol &= 7;
    NR50_REG = (uint8_t)((vol << 4) | vol);
}

void audio_init(void) NONBANKED
{
    uint8_t i;
    NR52_REG = 0x80;            /* APU on                                 */
    NR50_REG = 0x77;
    NR51_REG = 0xFF;
    nr51_shadow = 0xFF;
    NR10_REG = 0x00;
    NR11_REG = 0x80; NR21_REG = 0x80;
    /* power the DACs once at volume 0 so later notes start without a pop */
    NR12_REG = 0x08; NR14_REG = 0x80;
    NR22_REG = 0x08; NR24_REG = 0x80;
    NR42_REG = 0x08; NR44_REG = 0x80;
    NR30_REG = 0x00;
    for (i = 0; i < 16; i++) AUD3WAVE[i] = 0x88;
    NR30_REG = 0x80; NR32_REG = 0x00;
    wave_key = 0xFF;
    for (i = 0; i < NUM_CH; i++) { rt[i].active = 0; ch_meter[i] = 0; }
    aq_head = aq_tail = 0;
}
