#!/usr/bin/env python3
"""AETHER - by Synth Centric
Emulator test-suite: runs the real ROM in PyBoy (headless, sound emulated),
drives it with joypad input like a user would, and checks the screen
(background tile map), live engine state in WRAM and the audio output.

usage: python3 tests/emu_test.py build/AETHER_by_Synth_Centric.gb [--long MINUTES] [--demo SECONDS]

Writes build/emu/report.md, screenshots to build/emu/, and (with --demo)
WAV recordings of each factory composition to release/demo_audio/.
"""
import io, json, os, sys, time, wave, subprocess, shutil
import numpy as np
from pyboy import PyBoy

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ROM = sys.argv[1] if len(sys.argv) > 1 and not sys.argv[1].startswith("--") else os.path.join(ROOT, "build/AETHER_by_Synth_Centric.gb")
LONG_MIN = 60
DEMO_SEC = 0
args = sys.argv[1:]
if "--long" in args: LONG_MIN = int(args[args.index("--long") + 1])
if "--demo" in args: DEMO_SEC = int(args[args.index("--demo") + 1])
OUT = os.path.join(ROOT, "build/emu")
os.makedirs(OUT, exist_ok=True)

# ---------------------------------------------------------------- symbols
sym = {}
for line in open(ROM.replace(".gb", ".noi")):
    p = line.split()
    if len(p) == 3 and p[0] == "DEF":
        sym[p[1]] = int(p[2], 16)
offs_path = os.path.join(ROOT, "build/offsets.json")
if True:   # always regenerate: the layout must match the ROM being tested
    subprocess.check_call(["gcc", "-fpack-struct", "-I" + os.path.join(ROOT, "include"),
                           os.path.join(ROOT, "tests/offsets.c"), "-o", os.path.join(ROOT, "build/offsets")])
    open(offs_path, "w").write(subprocess.check_output([os.path.join(ROOT, "build/offsets")]).decode())
OFF = json.load(open(offs_path))
E, P = sym["_eng"], sym["_proj"]
SR = 48000

results = []
def check(name, cond, detail=""):
    results.append((name, bool(cond), detail))
    print(("  PASS " if cond else "  FAIL ") + name + (("  [" + str(detail) + "]") if detail != "" else ""))
    return cond

class Emu:
    def __init__(self, ram=None, sound=True):
        kw = dict(window="null", sound_emulated=sound, sound_sample_rate=SR)
        if ram is None:
            # a fresh cartridge: uninitialised battery RAM holds garbage
            ram = io.BytesIO(np.random.RandomState(7).randint(0, 256, 32768).astype(np.uint8).tobytes())
        self.pb = PyBoy(ROM, ram_file=ram, **kw)
        self.sound = sound
        self.audio = []
        self.rec = False

    def tick(self, n=1):
        for k in range(n):
            self.pb.tick(1, k == n - 1, self.sound)     # render the last frame
            if self.rec:
                self.audio.append(self.pb.sound.ndarray.copy())

    def press(self, b, frames=4):
        self.pb.button_press(b); self.tick(frames); self.pb.button_release(b); self.tick(6)

    def r8(self, a): return self.pb.memory[a]
    def r16(self, a): return self.pb.memory[a] | (self.pb.memory[a + 1] << 8)
    def eng8(self, f, i=0): return self.r8(E + OFF[f] + i)
    def eng16(self, f, i=0): return self.r16(E + OFF[f] + 2 * i)
    def proj8(self, f, i=0): return self.r8(P + OFF[f] + i)
    def sym8(self, s): return self.r8(sym[s])

    def row(self, y, raw=False):
        if not raw:
            return self.row(y, True).upper()     # the interface is lowercase; compare case-insensitively
        s = ""
        for x in range(20):
            t = self.pb.tilemap_background[x, y]
            t = t - 256 if t >= 256 else t
            if t < 95: s += chr(t + 32)
            elif t < 159: s += chr(t - 95 + 32)
            else: s += "~"
        return s

    def shot(self, name):
        self.pb.screen.image.convert("RGB").resize((320, 288), 0).save(os.path.join(OUT, name + ".png"))

    def take_audio(self):
        a = np.concatenate(self.audio) if self.audio else np.zeros((0, 2), np.int8)
        self.audio = []
        return a

    def stop(self, ram_out=None):
        if ram_out is not None: self.pb.stop(save=True, ram_file=ram_out)
        else: self.pb.stop(save=False)

def rms(a):
    if len(a) == 0: return 0.0
    x = a.astype(np.float64)
    x = x - x.mean(axis=0)
    return float(np.sqrt((x ** 2).mean()))

def write_wav(path, a):
    x = a.astype(np.float64)
    x -= x.mean(axis=0)
    peak = np.abs(x).max() or 1.0
    x = np.clip(x / peak * 0.89 * 32767, -32768, 32767).astype("<i2")
    with wave.open(path, "wb") as w:
        w.setnchannels(2); w.setsampwidth(2); w.setframerate(SR)
        w.writeframes(x.tobytes())

MENU_INDEX = {7: 0, 1: 1, 2: 2, 3: 3, 4: 4, 8: 5, 5: 6}   # page -> menu row
def goto_page(emu, page):
    """navigate like a user: SELECT opens the menu, pick the item with A"""
    if emu.sym8("_ui_page") == page: return True
    if emu.sym8("_ui_page") != 6:
        emu.press("select"); emu.tick(20)
    if page == 0:
        emu.press("select"); emu.tick(20)
        return emu.sym8("_ui_page") == 0
    for _ in range(8): emu.press("up")
    for _ in range(MENU_INDEX[page]): emu.press("down")
    emu.press("a"); emu.tick(25)
    return emu.sym8("_ui_page") == page

# ===================================================================== run
print("AETHER emulator tests  ROM:", ROM)
rom = open(ROM, "rb").read()
check("cartridge header: MBC5+RAM+BATTERY, 32KB SRAM, DMG mode",
      rom[0x147] == 0x1B and rom[0x149] == 0x03 and rom[0x143] in (0x00, 0x80) and rom[0x143] != 0xC0,
      "type=%02X rom=%02X ram=%02X cgb=%02X" % (rom[0x147], rom[0x148], rom[0x149], rom[0x143]))
check("ROM size matches header", len(rom) == 32768 << rom[0x148], len(rom))

emu = Emu()
emu.tick(150)
emu.shot("01_title")
title_rows = [emu.row(y, raw=True) for y in range(18)]
check("title: logo tiles drawn (AETHER)", all(192 <= ((emu.pb.tilemap_background[x, 3] - 256) % 512 if emu.pb.tilemap_background[x, 3] >= 256 else emu.pb.tilemap_background[x, 3]) for x in range(2, 18)))
check("title: 'by Synth Centric' directly beneath the logo", title_rows[7].strip() == "by Synth Centric", title_rows[7])
check("title: no inverted / tabbed header", all(t < 95 or t >= 160 for t in [emu.pb.tilemap_background[x, 0] % 256 if emu.pb.tilemap_background[x, 0] >= 256 else emu.pb.tilemap_background[x, 0] for x in range(20)]))
check("title: 'generative ambient instrument'", "generative ambient" in title_rows[11] and "instrument" in title_rows[12])
check("title: not playing before user input", emu.eng8("playing") == 0)

# ----- start playback from the title (START)
emu.press("start")
emu.tick(30)
check("START on title -> PLAY (mood pad)", emu.sym8("_ui_page") == 0 and "AETHER" in emu.row(0)[:10], emu.row(0))
check("playback started", emu.eng8("playing") == 1)
t0 = emu.r16(sym["_audio_now"]); ev0 = emu.eng16("st_events")
emu.rec = True
emu.tick(60 * 30)
emu.rec = False
a = emu.take_audio()
t1 = emu.r16(sym["_audio_now"]); ev1 = emu.eng16("st_events")
check("VBlank audio clock advances 1/frame", abs(((t1 - t0) & 0xFFFF) - 1800) <= 2, (t1 - t0) & 0xFFFF)
check("generative events produced (30 s)", ev1 - ev0 >= 10, ev1 - ev0)
check("audio output from DMG sound hardware", rms(a) > 0.3, "rms=%.2f" % rms(a))
emu.shot("02_live_playing")
live = [emu.row(y) for y in range(18)]
check("PLAY shows song, part, mood pad and button legend",
      "FIRST LIGHT" in live[2] and "TENSE" in live[7] and "SPARSE" in live[14] and "KEEP" in live[16] and "MENU" in live[17],
      live[16])

# ----- IMPRINT capture from LIVE
emu.press("a"); emu.tick(5)
mlen = emu.proj8("motif", 0)
check("IMPRINT capture (LIVE, A) stores a motif", mlen >= 2, "len=%d msg=%s" % (mlen, emu.row(17)))
check("IMPRINT confirmation shown", "IMPRINT 1 CAPTURED" in emu.row(17), emu.row(17))

# ----- mood pad: the d-pad shapes the music directly
d0, h0 = emu.proj8("macro", 0), emu.proj8("macro", 2)
emu.press("right"); emu.press("right"); emu.press("up")
d1, h1 = emu.proj8("macro", 0), emu.proj8("macro", 2)
check("mood pad: right = denser, up = more tension", d1 == min(15, d0 + 2) and h1 > h0, "density %d->%d harmony %d->%d" % (d0, d1, h0, h1))
sc0 = emu.eng8("form.scene")
emu.press("b"); emu.tick(10)
check("B on PLAY moves to the next part", emu.eng8("form.scene") != sc0 and "NEXT" in emu.row(17), emu.row(17))

# ----- menu navigation
emu.press("select"); emu.tick(20)
check("SELECT opens the menu", emu.sym8("_ui_page") == 6 and "SONGS" in emu.row(3), emu.row(3))
names = {7: "SONGS", 1: "SOUNDS", 2: "PLAYERS", 3: "KEY", 4: "SECTIONS", 8: "FINE TUNE", 5: "SAVE/LOAD"}
ok = True
for pg, nm in names.items():
    goto_page(emu, pg)
    if emu.sym8("_ui_page") != pg or nm not in emu.row(0): ok = False
    emu.shot("03_page_%d_%s" % (pg, nm.lower().replace("/", "_").replace(" ", "_")))
    emu.press("b"); emu.tick(20)
    if emu.sym8("_ui_page") != 6: ok = False
check("every menu item opens its page; B returns to the menu", ok)
emu.press("select"); emu.tick(20)
check("SELECT from the menu returns to the mood pad", emu.sym8("_ui_page") == 0)
check("playback continues while navigating", emu.eng8("playing") == 1)

# ----- songs: pick a piece and it plays
goto_page(emu, 7)
emu.press("down"); emu.press("down")
check("songs: description of the highlighted song", "UNSTABLE" in emu.row(16), emu.row(16))
emu.press("a"); emu.tick(30)
check("songs: A loads the song and returns to the pad, playing",
      emu.sym8("_ui_page") == 0 and "STATIC MEMORY" in emu.row(2) and emu.eng8("playing") == 1, emu.row(2))
emu.shot("03_song_loaded")

# ----- SYNTH: edit instrument 1 volume, audition, wave lab drawing
goto_page(emu, 1)
emu.press("a"); emu.tick(20)
emu.shot("04_pulse_lab")
check("instrument editor opens (PULSE LAB)", "TYPE" in emu.row(3) and "PULSE" in emu.row(3), emu.row(3))
for _ in range(4): emu.press("down")
check("help panel explains the selected parameter", "LOUDNESS" in emu.row(16), emu.row(16))
inst_vol_addr = P + OFF["inst"] + 0 * OFF["sizeof_inst"] + 2
v0 = emu.r8(inst_vol_addr)
emu.press("left")
check("PULSE LAB: VOLUME edit changes instrument 1", emu.r8(inst_vol_addr) == max(0, v0 - 1), "%d->%d" % (v0, emu.r8(inst_vol_addr)))
emu.press("right")
u0 = emu.eng16("st_ag_ev", 9)
emu.press("a"); emu.tick(5)
check("A auditions the edited instrument", emu.eng16("st_ag_ev", 9) == u0 + 1 or "BUSY" in emu.row(17), emu.row(17))
emu.press("b"); emu.tick(10)
for _ in range(8): emu.press("down")
emu.press("a"); emu.tick(30)
emu.shot("05_wave_lab")
check("WAVE LAB opens", "WAVE LAB" in emu.row(0), emu.row(0))
emu.press("a"); emu.tick(5)                       # DRAW
w_addr = P + OFF["inst"] + 8 * OFF["sizeof_inst"]  # proj.wave follows the instruments
w0 = emu.r8(w_addr) >> 4
emu.press("down"); emu.press("down"); emu.press("down")
w1 = emu.r8(w_addr) >> 4
check("WAVE LAB: D-pad draws sample 0 of wave 1", w1 == max(0, w0 - 3), "%d->%d" % (w0, w1))
emu.shot("06_wave_draw")
emu.press("b"); emu.press("b")

# ----- AGENTS: toggle, edit, ecology screen
goto_page(emu, 2)
en_addr = P + OFF["agent"] + 0 * OFF["sizeof_agent"] + 0
e0 = emu.r8(en_addr)
emu.press("left")
check("AGENTS: ORBIT enable toggles", emu.r8(en_addr) == (e0 ^ 1))
emu.press("left")
emu.press("a"); emu.tick(20)
emu.shot("07_agent_orbit")
act_addr = P + OFF["agent"] + 1
a0 = emu.r8(act_addr)
emu.press("down"); emu.press("left")
check("AGENTS: ORBIT activity editable", emu.r8(act_addr) == max(0, a0 - 1), "%d->%d" % (a0, emu.r8(act_addr)))
emu.press("right")
emu.press("b"); emu.tick(10)
for _ in range(8): emu.press("down")
emu.press("a"); emu.tick(20)
emu.shot("08_ecology")
check("ECOLOGY screen shows the four variables",
      emu.row(2).strip().startswith("ENERGY") and emu.row(5).strip().startswith("MEMORY"), emu.row(2))
emu.press("b")

# ----- HARMONY: change scale
goto_page(emu, 3)
s0 = emu.proj8("harm", 1)
emu.press("down"); emu.press("right")
check("HARMONY: scale selection", emu.proj8("harm", 1) == s0 + 1, emu.row(2))
emu.press("left")
emu.shot("09_harmony")
emu.press("b"); emu.tick(10)
check("B on an editor's top level returns to the menu", emu.sym8("_ui_page") == 6, emu.row(0))

# ----- FORM: FREE/CLOCKED and scene change
goto_page(emu, 4)
check("FORM: FIRST LIGHT runs in FREE time", emu.proj8("glob", 0) == 1)
emu.press("left"); emu.tick(5)
check("FORM: switch to CLOCKED", emu.proj8("glob", 0) == 0)
ev_a = emu.eng16("st_events"); emu.tick(600); ev_b = emu.eng16("st_events")
check("CLOCKED mode generates events", ev_b > ev_a, ev_b - ev_a)
emu.press("right")
check("FORM: back to FREE", emu.proj8("glob", 0) == 1)
for _ in range(4): emu.press("down")
target = (emu.eng8("form.scene") + 2) % 5
while emu.r8(sym["_ui_var"]) != target: emu.press("right")
emu.press("a"); emu.tick(10)
check("FORM: manual scene change", emu.eng8("form.scene") == target, "scene=%d" % emu.eng8("form.scene"))
emu.shot("10_form")

# ----- PROJECT: seed edit, save (with confirmation)
goto_page(emu, 5)
emu.press("down")
seed0 = emu.proj8("seed_lo") | emu.proj8("seed_lo", 1) << 8
for _ in range(5): emu.press("right")
seed1 = emu.proj8("seed_lo") | emu.proj8("seed_lo", 1) << 8
check("PROJECT: seed editable", seed1 == (seed0 + 5) & 0xFFFF, "%d->%d" % (seed0, seed1))
for _ in range(3): emu.press("down")              # SAVE row
emu.press("a"); emu.tick(5)
check("save asks for confirmation", "CONFIRM" in emu.row(17) or "AGAIN" in emu.row(17), emu.row(17))
emu.press("a"); emu.tick(30)
check("save verified", "SAVED" in emu.row(17), emu.row(17))
check("slot 1 reports OK", "OK" in emu.row(13), emu.row(13))
emu.shot("11_project_saved")
name_saved = bytes(emu.proj8("name", i) for i in range(18))
ram = io.BytesIO()
emu.stop(ram_out=ram)

# ----- cold boot with the battery RAM, load the project back
ram.seek(0)
emu = Emu(ram=io.BytesIO(ram.getvalue()))
emu.tick(150)
emu.press("a"); emu.tick(30)                       # title -> LIVE (not playing)
seed_boot = emu.proj8("seed_lo") | emu.proj8("seed_lo", 1) << 8
goto_page(emu, 5)
for _ in range(5): emu.press("down")               # LOAD row
emu.press("a"); emu.press("a"); emu.tick(30)
seed_loaded = emu.proj8("seed_lo") | emu.proj8("seed_lo", 1) << 8
check("power cycle + LOAD restores the saved project", seed_loaded == seed1 and seed_boot != seed1,
      "boot=%d loaded=%d saved=%d" % (seed_boot, seed_loaded, seed1))
check("loaded name matches", bytes(emu.proj8("name", i) for i in range(18)) == name_saved)

# ----- stop leaves no stuck notes
emu.press("start"); emu.tick(60 * 10)
check("playback after load", emu.eng8("playing") == 1)
emu.press("start"); emu.tick(20)
emu.rec = True; emu.tick(120); emu.rec = False
quiet = emu.take_audio()
check("STOP silences all channels (no stuck notes)", rms(quiet) < 0.05, "rms=%.3f" % rms(quiet))
emu.stop()

# ----- extended playback (real emulated time)
print("  extended playback: %d emulated minutes..." % LONG_MIN)
emu = Emu(sound=True)
emu.tick(150); emu.press("start")
t_start = time.time()
samples = []
prev_ev = emu.eng16("st_events")
total_ev = 0
silent_windows = 0
for m in range(LONG_MIN):
    emu.rec = (m % 10 == 9)
    emu.tick(3584)
    if emu.rec:
        a = emu.take_audio(); emu.rec = False
        if rms(a) < 0.05: silent_windows += 1
    ev = emu.eng16("st_events")
    total_ev += (ev - prev_ev) & 0xFFFF
    prev_ev = ev
    if m % 10 == 9:
        samples.append(dict(minute=m + 1, events=total_ev, scene=emu.eng8("form.scene"),
                            transitions=emu.eng8("form.transitions"),
                            E=emu.eng16("eco.v", 0) >> 8, T=emu.eng16("eco.v", 1) >> 8,
                            S=emu.eng16("eco.v", 2) >> 8, M=emu.eng16("eco.v", 3) >> 8))
hours, secs = emu.eng8("form.hours"), emu.eng16("form.total_s")
check("extended playback still running after %d min" % LONG_MIN, emu.eng8("playing") == 1)
check("form clock tracks emulated time", abs(hours * 3600 + secs - LONG_MIN * 60) <= LONG_MIN, "%dh %ds" % (hours, secs))
check("music continues throughout (events every 10-min window)", all(s["events"] > 0 for s in samples) and silent_windows == 0, silent_windows)
check("long-form development (scene transitions)", emu.eng8("form.transitions") >= LONG_MIN // 6, emu.eng8("form.transitions"))
check("no event queue overflow", emu.eng16("st_qfull") == 0, emu.eng16("st_qfull"))
gen_max, ui_max, isr = emu.sym8("_prof_gen_max"), emu.sym8("_prof_ui_max"), emu.sym8("_isr_lines")
# per-frame profile of the VBlank driver over 2 minutes of playback
isr_samples = []
for f in range(60 * 120):
    emu.pb.memory[sym["_isr_lines"]] = 0
    emu.tick(1)
    isr_samples.append(emu.sym8("_isr_lines"))
isr_avg = sum(isr_samples) / len(isr_samples)
gen_samples = []
ui_samples = []
for f in range(60 * 60):
    emu.tick(1)
    gen_samples.append(emu.sym8("_prof_gen_lines"))
    ui_samples.append(emu.sym8("_prof_ui_lines"))
gen_avg = sum(gen_samples) / len(gen_samples)
gen_p99 = sorted(gen_samples)[int(len(gen_samples) * 0.99)]
ui_avg = sum(ui_samples) / len(ui_samples)
check("main loop keeps up: generator + UI + driver < 1 frame on average", gen_avg + ui_avg + isr_avg < 154,
      "gen %.1f + ui %.1f + isr %.1f lines of 154" % (gen_avg, ui_avg, isr_avg))
isr_p99 = sorted(isr_samples)[int(len(isr_samples) * 0.99)]
check("VBlank driver: average cost < 15% of a frame", isr_avg < 23, "avg %.1f lines of 154" % isr_avg)
check("VBlank driver: worst frame < 50% of a frame", max(isr_samples) < 77 and isr < 77, "max %d, p99 %d" % (max(isr_samples), isr_p99))
check("generator max lag within look-ahead", emu.eng8("st_maxbehind") <= 6, emu.eng8("st_maxbehind"))
print("  long run took %.1f s wall clock" % (time.time() - t_start))
long_stats = dict(minutes=LONG_MIN, events=total_ev, transitions=emu.eng8("form.transitions"),
                  gen_lines_max=gen_max, ui_lines_max=ui_max, isr_lines_max=isr,
                  isr_lines_avg=round(isr_avg, 1), isr_lines_p99=isr_p99,
                  gen_lines_avg=round(gen_avg, 1), gen_lines_p99=gen_p99, ui_lines_avg=round(ui_avg, 1),
                  max_behind=emu.eng8("st_maxbehind"), dropped=emu.eng16("st_dropped"),
                  stolen=emu.eng16("st_stolen"), windows=samples)
emu.shot("12_after_long_run")
emu.stop()

# ----- all eight agents simultaneously (dense stress) in the emulator
emu = Emu(sound=False)
emu.tick(150); emu.press("a"); emu.tick(20)
for i in range(8):
    emu.pb.memory[P + OFF["agent"] + i * OFF["sizeof_agent"] + 0] = 1
    emu.pb.memory[P + OFF["agent"] + i * OFF["sizeof_agent"] + 1] = 15
for s in range(8):
    emu.pb.memory[P + OFF["scene"] + s * 30 + 0] = 0xFF
for k in range(6): emu.pb.memory[P + OFF["macro"] + k] = 15
emu.pb.memory[E + OFF["form.live"] + 0] = 0xFF        # running scene copy: all agents
emu.press("start")
emu.tick(60 * 60 * 5)
check("8 agents at max activity for 5 min: still running", emu.eng8("playing") == 1)
check("8 agents: no queue overflow", emu.eng16("st_qfull") == 0, emu.eng16("st_qfull"))
dense = dict(gen_lines_max=emu.sym8("_prof_gen_max"), ui_lines_max=emu.sym8("_prof_ui_max"),
             isr_lines_max=emu.sym8("_isr_lines"), max_behind=emu.eng8("st_maxbehind"),
             events=emu.eng16("st_events"), dropped=emu.eng16("st_dropped"))
check("8 agents: generator keeps within look-ahead", emu.eng8("st_maxbehind") <= 6, emu.eng8("st_maxbehind"))
emu.shot("13_dense_live")
emu.stop()

# ----- demo audio: every factory composition, recorded from the ROM
demos = []
if DEMO_SEC:
    dd = os.path.join(ROOT, "release/demo_audio")
    os.makedirs(dd, exist_ok=True)
    names = ["first_light", "pale_transmission", "static_memory", "dissolving_light", "empty_rooms",
             "glass_orbit", "night_current", "soft_machines", "afterimage"]
    for idx, nm in enumerate(names):
        emu = Emu(sound=True)
        emu.tick(150); emu.press("a"); emu.tick(20)
        goto_page(emu, 5)
        for _ in range(7): emu.press("down")      # FACTORY row
        for _ in range(idx): emu.press("right")
        emu.press("a"); emu.press("a"); emu.tick(10)
        goto_page(emu, 0)
        emu.press("start")
        emu.rec = True
        emu.tick(int(DEMO_SEC * 59.73))
        emu.rec = False
        a = emu.take_audio()
        path = os.path.join(dd, "%02d_%s.wav" % (idx, nm))
        write_wav(path, a)
        # activity: share of 2-second windows that are not silent
        x = a.astype(np.float64); x -= np.median(x, axis=0)
        win = 2 * SR
        nwin = max(1, len(x) // win)
        active = sum(1 for k in range(nwin) if np.abs(x[k * win:(k + 1) * win]).max() > 0.5) / nwin
        if shutil.which("ffmpeg"):
            flac = path[:-4] + ".flac"
            subprocess.run(["ffmpeg", "-loglevel", "error", "-y", "-i", path, "-c:a", "flac", flac], check=True)
            os.remove(path)
            path = flac
        demos.append((os.path.basename(path), len(a) / SR, rms(a), active))
        check("demo %s recorded from ROM (%ds)" % (nm, DEMO_SEC), rms(a) > 0.1, "rms=%.2f active=%.0f%%" % (rms(a), active * 100))
        if idx in (5, 6, 7):   # pulse-driven CLOCKED pieces must sound most of the time
            check("demo %s: pulse figures audible" % nm, active > 0.6, "%.0f%% of 2 s windows" % (active * 100))
        emu.shot("14_demo_%s" % nm)
        emu.stop()

# ---------------------------------------------------------------- report
passed = sum(1 for r in results if r[1])
with open(os.path.join(OUT, "report.md"), "w") as f:
    f.write("# AETHER emulator test report\n\n")
    f.write("Emulator: PyBoy %s (headless, sound emulated at %d Hz). ROM: `%s`.\n\n" % (
        __import__("pyboy").__version__ if hasattr(__import__("pyboy"), "__version__") else "2.x", SR, os.path.basename(ROM)))
    f.write("Result: **%d / %d checks passed**\n\n| check | result | detail |\n|---|---|---|\n" % (passed, len(results)))
    for n, ok, d in results:
        f.write("| %s | %s | %s |\n" % (n, "PASS" if ok else "FAIL", str(d).replace("|", "/")))
    f.write("\n## Extended playback (%d emulated minutes, FIRST LIGHT)\n\n```\n%s\n```\n" % (LONG_MIN, json.dumps(long_stats, indent=1)))
    f.write("\n## Dense stress (all 8 agents, all macros 15, 5 emulated minutes)\n\n```\n%s\n```\n" % json.dumps(dense, indent=1))
    if demos:
        f.write("\n## Demo recordings (actual ROM output)\n\n")
        for n, sec, r, act in demos: f.write("- `%s` %.0f s, rms %.2f, sounding in %.0f%% of 2-s windows\n" % (n, sec, r, act * 100))
print("%d / %d emulator checks passed" % (passed, len(results)))
sys.exit(0 if passed == len(results) else 1)
