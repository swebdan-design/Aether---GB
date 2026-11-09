/*
 * AETHER - by Synth Centric
 * save.c : versioned, checksummed, redundant project storage.
 *
 * SRAM layout (32 KB, 4 banks of 8 KB):
 *   slot s, copy c (0=A,1=B) at linear address (s*2 + c) * 2048
 *   +0  'A' 'E' 'T' 'H'  magic (written last = commit marker)
 *   +4  format version
 *   +5  generation counter (mod 256, newer wins)
 *   +6  payload size (lo, hi)
 *   +8  Fletcher-16 of payload (lo, hi)
 *   +10 Fletcher-16 of header bytes 4..9 (lo, hi)
 *   +12 reserved (4)
 *   +16 payload: Project structure (PROJECT_SIZE bytes)
 *
 * Validation never trusts uninitialised SRAM: a copy is valid only with
 * the magic, a known version, the exact size and both checksums.  A bad
 * slot is reported as CORRUPT and is never touched unless the user saves
 * into it; other slots are never affected.
 */
#pragma bank 255
#include <string.h>
#include "storage.h"
#include "engine.h"

static const uint8_t magic[4] = { 'A', 'E', 'T', 'H' };

uint16_t fletcher16(const uint8_t *d, uint16_t n) BANKED
{
    uint16_t a = 0, b = 0;
    while (n--) {
        a += *d++;
        if (a >= 255) a -= 255;
        b += a;
        if (b >= 255) b -= 255;
    }
    return (uint16_t)((b << 8) | a);
}

static uint16_t copy_addr(uint8_t slot, uint8_t c)
{
    return (uint16_t)(((uint16_t)(slot & 7) * 2 + (c & 1)) * COPY_SIZE);
}

/* streaming checksum of a payload stored in SRAM */
static uint16_t sram_sum(uint16_t addr, uint16_t n)
{
    uint8_t buf[32];
    uint16_t a = 0, b = 0, i;
    while (n) {
        uint16_t k = n > 32 ? 32 : n;
        sram_rd(addr, buf, k);
        for (i = 0; i < k; i++) {
            a += buf[i];
            if (a >= 255) a -= 255;
            b += a;
            if (b >= 255) b -= 255;
        }
        addr += k;
        n -= k;
    }
    return (uint16_t)((b << 8) | a);
}

/* returns 0 blank, 1 valid (gen in *gen), 2 damaged header,
 * 3 valid header but damaged payload (gen in *gen) */
static uint8_t check_copy(uint8_t slot, uint8_t c, uint8_t *gen)
{
    uint8_t h[HDR_SIZE];
    uint16_t addr = copy_addr(slot, c);
    uint16_t size, sum;
    sram_rd(addr, h, HDR_SIZE);
    if (memcmp(h, magic, 4) != 0) {
        /* any non-erased byte in the header suggests an interrupted write */
        uint8_t i, blank = 1;
        for (i = 0; i < HDR_SIZE; i++) if (h[i] != 0xFF && h[i] != 0x00) blank = 0;
        return blank ? 0 : 2;
    }
    if (h[4] != SAVE_FORMAT_VERSION) return 2;
    if (fletcher16(&h[4], 6) != (uint16_t)(h[10] | ((uint16_t)h[11] << 8))) return 2;
    size = (uint16_t)(h[6] | ((uint16_t)h[7] << 8));
    if (size != PROJECT_SIZE) return 2;
    sum = (uint16_t)(h[8] | ((uint16_t)h[9] << 8));
    *gen = h[5];                    /* header is trustworthy from here on */
    if (sram_sum((uint16_t)(addr + HDR_SIZE), size) != sum) return 3;
    return 1;
}

/* newer of two generation counters (mod 256) */
static uint8_t newer(uint8_t a, uint8_t b) { return (int8_t)(a - b) > 0; }

/* *bad: bit0/1 = copy A/B damaged, bit2 = a damaged copy carries a newer
 * generation than the valid one (the latest save was lost) */
static uint8_t best_copy(uint8_t slot, uint8_t *which, uint8_t *gen, uint8_t *bad)
{
    uint8_t ga = 0, gb = 0;
    uint8_t va = check_copy(slot, 0, &ga);
    uint8_t vb = check_copy(slot, 1, &gb);
    *bad = (uint8_t)((va >= 2) | ((vb >= 2) << 1));
    if (va == 3 && vb == 1 && newer(ga, gb)) *bad |= 4;
    if (vb == 3 && va == 1 && newer(gb, ga)) *bad |= 4;
    if (va == 1 && vb == 1) { *which = newer(gb, ga) ? 1 : 0; *gen = *which ? gb : ga; return 1; }
    if (va == 1) { *which = 0; *gen = ga; return 1; }
    if (vb == 1) { *which = 1; *gen = gb; return 1; }
    return 0;
}

uint8_t slot_status(uint8_t slot) BANKED
{
    uint8_t w, g, bad;
    if (best_copy(slot, &w, &g, &bad)) return (bad & 4) ? SLOT_BACKUP : SLOT_OK;
    return bad ? SLOT_CORRUPT : SLOT_EMPTY;
}

uint8_t slot_name(uint8_t slot, char *out) BANKED
{
    uint8_t w, g, bad;
    if (!best_copy(slot, &w, &g, &bad)) { out[0] = 0; return 0; }
    sram_rd((uint16_t)(copy_addr(slot, w) + HDR_SIZE), (uint8_t *)out, PROJ_NAME_LEN);
    out[PROJ_NAME_LEN] = 0;
    return 1;
}

uint8_t save_slot(uint8_t slot) BANKED
{
    uint8_t w, g = 0, bad, target, h[HDR_SIZE], chk;
    uint16_t addr, sum, hs;
    if (best_copy(slot, &w, &g, &bad)) { target = w ^ 1; g++; }
    else {
        /* nothing valid: keep generations monotonic past any readable header */
        uint8_t ga = 0, gb = 0;
        uint8_t va = check_copy(slot, 0, &ga), vb = check_copy(slot, 1, &gb);
        target = 0;
        g = 1;
        if (va == 3) g = (uint8_t)(ga + 1);
        if (vb == 3 && (va != 3 || newer(gb, ga))) g = (uint8_t)(gb + 1);
    }
    /* never leave a damaged copy that claims to be newer than this save */
    {
        uint8_t go = 0;
        if (check_copy(slot, (uint8_t)(target ^ 1), &go) == 3 && !newer(g, go)) g = (uint8_t)(go + 1);
    }
    addr = copy_addr(slot, target);

    /* 1. invalidate target magic first, 2. payload, 3. header, 4. magic */
    memset(h, 0, HDR_SIZE);
    sram_wr(addr, h, 4);
    sram_wr((uint16_t)(addr + HDR_SIZE), (const uint8_t *)&proj, PROJECT_SIZE);
    sum = fletcher16((const uint8_t *)&proj, PROJECT_SIZE);
    h[4] = SAVE_FORMAT_VERSION;
    h[5] = g;
    h[6] = (uint8_t)(PROJECT_SIZE & 0xFF);
    h[7] = (uint8_t)(PROJECT_SIZE >> 8);
    h[8] = (uint8_t)sum;
    h[9] = (uint8_t)(sum >> 8);
    hs = fletcher16(&h[4], 6);
    h[10] = (uint8_t)hs;
    h[11] = (uint8_t)(hs >> 8);
    sram_wr((uint16_t)(addr + 4), &h[4], HDR_SIZE - 4);
    sram_wr(addr, magic, 4);

    /* verify by reading back */
    return check_copy(slot, target, &chk) == 1 && chk == g;
}

uint8_t load_slot(uint8_t slot) BANKED
{
    uint8_t w, g, bad;
    if (!best_copy(slot, &w, &g, &bad)) return 0;
    sram_rd((uint16_t)(copy_addr(slot, w) + HDR_SIZE), (uint8_t *)&proj, PROJECT_SIZE);
    return 1;
}

void slot_erase(uint8_t slot) BANKED
{
    uint8_t z[HDR_SIZE];
    memset(z, 0xFF, HDR_SIZE);
    sram_wr(copy_addr(slot, 0), z, HDR_SIZE);
    sram_wr(copy_addr(slot, 1), z, HDR_SIZE);
}
