#!/usr/bin/env python3
"""AETHER - by Synth Centric
Builds src/interface/gfx_data.c from assets/graphics/font5x7.txt plus
procedurally drawn instrument graphics (meters, indicators, logo).
"""
import math, sys, os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

def load_font(path):
    glyphs = {}
    cur = None
    rows = []
    for line in open(path):
        line = line.rstrip("\n")
        if line.startswith("#") and cur is None and not rows:
            continue
        if line.startswith("= "):
            if cur is not None:
                glyphs[cur] = rows
            name = line[2:]
            cur = " " if name == "space" else name
            rows = []
        elif cur is not None and line:
            rows.append(line)
    if cur is not None:
        glyphs[cur] = rows
    return glyphs

font = load_font(os.path.join(ROOT, "assets/graphics/font5x7.txt"))
assert len(font) == 95, len(font)

def glyph_bitmap(c):
    rows = font[c]
    bm = []
    for r in range(8):
        s = rows[r] if r < len(rows) else "....."
        bits = 0
        for x, ch in enumerate(s):
            if ch == "#":
                bits |= 0x80 >> (x + 1)      # column 1..5 of the 8x8 cell
        bm.append(bits)
    return bm

font1 = []
for code in range(32, 127):
    font1 += glyph_bitmap(chr(code))

def tile2bpp(pix):
    """pix: 8 rows of 8 colour indices -> 16 bytes"""
    out = []
    for r in range(8):
        lo = hi = 0
        for x in range(8):
            c = pix[r][x]
            if c & 1: lo |= 0x80 >> x
            if c & 2: hi |= 0x80 >> x
        out += [lo, hi]
    return out

def blank():
    return [[0] * 8 for _ in range(8)]

gfx = []
# horizontal bars 0..8
for n in range(9):
    p = blank()
    for x in range(8):
        for y in range(2, 6):
            if x < n: p[y][x] = 3
            elif y in (3, 4) and x % 2 == 0: p[y][x] = 1
    gfx.append(p)
# vertical bars 0..8 (fader / meter): solid fill over a thin centre track
for n in range(9):
    p = blank()
    for y in range(8):
        for x in range(1, 7):
            if 7 - y < n: p[y][x] = 3
            elif x in (3, 4): p[y][x] = 1
    gfx.append(p)
def circle(r, fill):
    p = blank()
    for y in range(8):
        for x in range(8):
            d = math.hypot(x - 3.5, y - 3.5)
            if fill and d <= r: p[y][x] = 3
            elif not fill and r - 1.0 <= d <= r: p[y][x] = 3
    return p
gfx.append(circle(3.2, True))     # DOT_ON
gfx.append(circle(3.2, False))    # DOT_OFF
p = blank(); p[3][3] = p[3][4] = p[4][3] = p[4][4] = 1; gfx.append(p)  # DOT_DIM
p = blank()                        # PLAY
for y in range(1, 7):
    w = 3 - abs(y - 3.5) + 0.5
    for x in range(2, 2 + int(w * 1.4) + 1): p[y][x] = 3
gfx.append(p)
p = blank()                        # STOP
for y in range(2, 6):
    for x in range(2, 6): p[y][x] = 3
gfx.append(p)
p = blank()                        # CURSOR (solid right triangle)
for y in range(0, 7):
    w = 3 - abs(y - 3)
    for x in range(1, 2 + w): p[y][x] = 3
gfx.append(p)
p = blank()                        # LARROW
for y in range(2, 6):
    w = 1 - abs(y - 3.5) + 0.5
    for x in range(5 - int(w * 2) - 1, 6): p[y][x] = 2
gfx.append(p)
p = blank()                        # RARROW
for y in range(2, 6):
    w = 1 - abs(y - 3.5) + 0.5
    for x in range(1, 3 + int(w * 2)): p[y][x] = 2
gfx.append(p)
p = blank()                        # HLINE
for x in range(8): p[4][x] = 1 if x % 2 else 2
gfx.append(p)
gfx.append(circle(2.6, True))     # PAGE_ON
p = circle(2.6, False)            # PAGE_OFF (grey ring)
for y in range(8):
    for x in range(8):
        if p[y][x]: p[y][x] = 1
gfx.append(p)
p = blank()                        # NOTE
for y in range(0, 6): p[y][5] = 3
p[0][6] = p[1][7] = 3
for (x, y) in [(3, 5), (4, 5), (3, 6), (4, 6), (2, 6), (2, 5)]: p[y][x] = 3
gfx.append(p)
p = blank()                        # BOX_ON
for y in range(1, 7):
    for x in range(1, 7):
        p[y][x] = 3 if (y in (1, 6) or x in (1, 6) or (2 < y < 5 and 2 < x < 5)) else 0
gfx.append(p)
p = blank()                        # BOX_OFF
for y in range(1, 7):
    for x in range(1, 7):
        p[y][x] = 2 if (y in (1, 6) or x in (1, 6)) else 0
gfx.append(p)
assert len(gfx) == 32, len(gfx)

# ---- extra glyphs for the v1.1 interface (tiles 154..159) ----
extra = []
p = blank()                        # UP (small triangle)
for y in range(2, 6):
    for x in range(4 - (y - 2), 4 + (y - 2)): p[y][x] = 2
extra.append(p)
p = blank()                        # DOWN
for y in range(2, 6):
    for x in range(4 - (5 - y), 4 + (5 - y)): p[y][x] = 2
extra.append(p)
p = blank()                        # RULE (fine dotted line)
for x in range(0, 8, 2): p[4][x] = 1
extra.append(p)
p = blank()                        # ORB (ring with a satellite)
for y in range(8):
    for x in range(8):
        d = math.hypot(x - 3.5, y - 4.0)
        if 1.6 <= d <= 2.7: p[y][x] = 3
p[0][7] = p[1][6] = 2
extra.append(p)
p = blank()                        # MARK (fader cursor, solid triangle)
for y in range(3, 7):
    for x in range(4 - (y - 3), 4 + (y - 3) + 1): p[y][x] = 3
extra.append(p)
p = blank()                        # DOTSM (small solid dot)
for (x, y) in [(3, 3), (4, 3), (3, 4), (4, 4)]: p[y][x] = 3
extra.append(p)

# ---- title logo: AETHER, font scaled x3 with scanline rendering ----
W, H = 128, 24
logo = [[0] * W for _ in range(H)]
x0 = 4
for i, ch in enumerate("AETHER"):
    rows = font[ch]
    for gy in range(7):
        for gx in range(5):
            if rows[gy][gx] == "#":
                for dy in range(3):
                    for dx in range(3):
                        X = x0 + i * 21 + gx * 3 + dx
                        Y = 1 + gy * 3 + dy
                        logo[Y][X] = 3 if dy < 2 else 1
logo_tiles = []
for ty in range(3):
    for tx in range(16):
        logo_tiles.append([[logo[ty * 8 + y][tx * 8 + x] for x in range(8)] for y in range(8)])

# ---- wave ornament: a slow sine over a dotted axis ----
wave = [[0] * W for _ in range(8)]
for x in range(W):
    if x % 4 == 0: wave[4][x] = 1
    env = math.sin(math.pi * x / (W - 1))
    y = 4 - round(3.2 * env * math.sin(2 * math.pi * x / 42.0))
    wave[max(0, min(7, y))][x] = 2 if 0.15 < env else 1
wave_tiles = [[[wave[y][tx * 8 + x] for x in range(8)] for y in range(8)] for tx in range(16)]

def emit_bytes(name, data, per=16):
    s = ["const uint8_t %s[%d] = {" % (name, len(data))]
    for i in range(0, len(data), per):
        s.append("    " + ", ".join("0x%02X" % b for b in data[i:i + per]) + ",")
    s.append("};")
    return "\n".join(s)

out = ["/* AETHER - by Synth Centric",
       " * GENERATED by tools/mkgfx.py from assets/graphics - do not edit. */",
       "#pragma bank 255",
       "#include <gb/gb.h>",
       "#include <stdint.h>",
       "BANKREF(gfx_data)",
       emit_bytes("font_1bpp", font1, 8),
       emit_bytes("gfx_2bpp", sum((tile2bpp(t) for t in gfx), [])),
       emit_bytes("extra_2bpp", sum((tile2bpp(t) for t in extra), [])),
       emit_bytes("logo_2bpp", sum((tile2bpp(t) for t in logo_tiles), [])),
       emit_bytes("wave_2bpp", sum((tile2bpp(t) for t in wave_tiles), [])),
       ""]
path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "src/interface/gfx_data.c")
open(path, "w").write("\n".join(out))
print("wrote", path)
