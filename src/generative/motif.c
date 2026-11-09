/*
 * AETHER - by Synth Centric
 * motif.c : IMPRINT system and motif library.
 *
 * A motif stores up to 8 notes as relative pitch (semitones from a base),
 * relative onset delay and duration (both in 4-frame units) plus a
 * playback instrument, time scale and recurrence weight.  User captures
 * (IMPRINT) and manually entered motifs live in the project and are
 * saved; MEMORY keeps a separate runtime library of automatic captures.
 */
#pragma bank 255
#include <string.h>
#include "engine.h"

static uint8_t gather(Motif *m, uint8_t agent_filter, uint8_t count)
{
    uint8_t idx[MOTIF_MAX], got = 0, k;
    uint16_t last_t = 0;
    if (count > MOTIF_MAX) count = MOTIF_MAX;
    if (count < 2) count = 2;
    for (k = 0; k < eng.hcount && got < count; k++) {
        uint8_t h = HIST_IDX(k);
        if (agent_filter == 0xFF || eng.hist[h].agent == agent_filter) idx[got++] = h;
    }
    if (got < 2) return 0;
    memset(m, 0, sizeof(Motif));
    for (k = 0; k < got; k++) {
        HistEv *h = &eng.hist[idx[got - 1 - k]];
        if (k == 0) { m->base = h->note; m->n[0].dt = 0; }
        else {
            uint16_t dt = (uint16_t)((h->time - last_t) >> 2);
            m->n[k].dt = (uint8_t)(dt > 255 ? 255 : (dt ? dt : 1));
        }
        last_t = h->time;
        m->n[k].rel = (int8_t)(h->note - m->base);
        m->n[k].dur = h->dur4 ? h->dur4 : 1;
    }
    {
        HistEv *h = &eng.hist[idx[0]];
        m->inst = h->agent < NUM_AGENTS ? proj.agent[h->agent].a[AP_INST] : 0;
        if (proj.inst[m->inst].p[IP_TYPE] == IT_NOISE) m->inst = 0;
    }
    m->len = got;
    m->tscale = 2;
    m->recur = 8;
    m->source = 8;
    return got;
}

uint8_t motif_capture(uint8_t slot, uint8_t agent_filter, uint8_t count) BANKED
{
    Motif tmp;
    uint8_t n = gather(&tmp, agent_filter, count);
    if (!n) return 0;
    memcpy(&proj.motif[slot % NUM_MOTIFS], &tmp, sizeof(Motif));
    ECO_IMPULSE(ECO_MEMORY, 20);
    return n;
}

void motif_auto_capture(uint8_t agent) BANKED
{
    Motif tmp;
    if (gather(&tmp, agent, (uint8_t)(4 + (eng.ticks & 3)))) {
        memcpy(&eng.amotif[eng.apos], &tmp, sizeof(Motif));
        eng.amotif[eng.apos].recur = 4;
        eng.apos = (eng.apos + 1) % NUM_AUTO_MOTIFS;
    }
}

void motif_clear(Motif *m) BANKED
{
    memset(m, 0, sizeof(Motif));
    m->tscale = 2;
    m->source = 8;
}

void motif_mutate(Motif *m, Rng *r) BANKED
{
    uint8_t k, op;
    if (m->len < 2) return;
    op = rng_below(r, 5);
    k = (uint8_t)(1 + rng_below(r, (uint8_t)(m->len - 1)));
    switch (op) {
    case 0:                                         /* nudge one interval */
        m->n[k].rel = (int8_t)(m->n[k].rel + rng_spread(r, 2));
        if (m->n[k].rel == m->n[k - 1].rel) m->n[k].rel++;
        break;
    case 1: {                                       /* swap two pitches   */
        int8_t t = m->n[k].rel;
        uint8_t j = (uint8_t)(k + 1 < m->len ? k + 1 : k - 1);
        m->n[k].rel = m->n[j].rel; m->n[j].rel = t;
        break; }
    case 2: {                                       /* rhythmic nudge     */
        int16_t d = (int16_t)m->n[k].dt + rng_spread(r, 3) * 2;
        m->n[k].dt = (uint8_t)(d < 1 ? 1 : d > 255 ? 255 : d);
        break; }
    case 3:                                         /* contour inversion  */
        for (k = 1; k < m->len; k++) m->n[k].rel = (int8_t)-m->n[k].rel;
        break;
    default: {                                      /* duration change    */
        int16_t d = (int16_t)m->n[k].dur + rng_spread(r, 4);
        m->n[k].dur = (uint8_t)(d < 1 ? 1 : d > 255 ? 255 : d);
        break; }
    }
}

void motif_transpose(Motif *m, int8_t semis) BANKED
{
    int16_t b = (int16_t)m->base + semis;
    if (b < 24) b = 24;
    if (b > 100) b = 100;
    m->base = (uint8_t)b;
}

/* 0 = compress time, 1 = expand time, 2 = thin (remove a note),
 * 3 = fill (insert a passing note in the widest leap) */
void motif_rhythm(Motif *m, uint8_t op) BANKED
{
    uint8_t k, best = 0, bw = 0;
    if (op == 0) { if (m->tscale > 1) m->tscale--; return; }
    if (op == 1) { if (m->tscale < 4) m->tscale++; return; }
    if (op == 2) {
        if (m->len <= 2) return;
        for (k = 1; k < m->len; k++) if (!best || m->n[k].dur < m->n[best].dur) best = k;
        if (best + 1 < m->len) m->n[best + 1].dt = (uint8_t)(m->n[best + 1].dt + m->n[best].dt > 255 ? 255 : m->n[best + 1].dt + m->n[best].dt);
        for (k = best; k + 1 < m->len; k++) m->n[k] = m->n[k + 1];
        m->len--;
        return;
    }
    if (m->len >= MOTIF_MAX || m->len < 2) return;
    for (k = 1; k < m->len; k++) {
        int8_t d = (int8_t)(m->n[k].rel - m->n[k - 1].rel);
        uint8_t ad = (uint8_t)(d < 0 ? -d : d);
        if (ad >= bw) { bw = ad; best = k; }
    }
    for (k = m->len; k > best; k--) m->n[k] = m->n[k - 1];
    m->n[best].rel = (int8_t)((m->n[best - 1].rel + m->n[best + 1].rel) / 2);
    m->n[best].dt = (uint8_t)((m->n[best + 1].dt >> 1) ? (m->n[best + 1].dt >> 1) : 1);
    m->n[best + 1].dt = (uint8_t)(m->n[best + 1].dt - (m->n[best + 1].dt >> 1));
    if (!m->n[best + 1].dt) m->n[best + 1].dt = 1;
    m->n[best].dur = m->n[best].dt;
    m->len++;
}

void motif_play(Motif *m, uint8_t agent, uint16_t delay) BANKED
{
    if (!m->len) return;
    phrase_start(m->n, m->len, m->base, agent, m->inst, 0, m->tscale, delay, 0);
}

Motif *motif_pick(Rng *r, uint8_t agent) BANKED
{
    uint8_t w[NUM_MOTIFS + NUM_AUTO_MOTIFS], k;
    uint16_t total = 0, pick;
    for (k = 0; k < NUM_MOTIFS; k++) {
        w[k] = proj.motif[k].len ? (uint8_t)(proj.motif[k].recur + (proj.motif[k].source == agent ? 6 : 0)) : 0;
        total += w[k];
    }
    for (k = 0; k < NUM_AUTO_MOTIFS; k++) {
        w[NUM_MOTIFS + k] = eng.amotif[k].len ? eng.amotif[k].recur : 0;
        total += w[NUM_MOTIFS + k];
    }
    if (!total) return 0;
    pick = (uint16_t)(((uint32_t)rng16(r) * total) >> 16);
    for (k = 0; k < NUM_MOTIFS + NUM_AUTO_MOTIFS; k++) {
        if (pick < w[k]) return k < NUM_MOTIFS ? &proj.motif[k] : &eng.amotif[k - NUM_MOTIFS];
        pick -= w[k];
    }
    return 0;
}
