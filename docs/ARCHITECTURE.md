# AETHER — Architecture

**AETHER — by Synth Centric** · generative ambient instrument for the original Nintendo Game Boy (DMG-01)

This document describes the software as it is actually implemented in v1.0.0.

---

## 1. Overview

```
             ┌──────────────────────────── main loop (cooperative) ─────────────────────────┐
 joypad ──►  │ input_isr latch ─► ui_frame()  : pages, editors, incremental screen updates  │
             │                    engine_service(): generate musical decisions up to       │
             │                                     LOOKAHEAD (6) frames ahead              │
             └───────────────────────────────┬──────────────────────────────────────────────┘
                                             │ NoteEvent queue (32 entries, SPSC, lock-free)
                                             ▼
             ┌──────────────── VBlank interrupt, 59.73 Hz, fixed bank 0 ─────────────────────┐
             │ audio_isr(): pop due events → trigger DMG channels (NR10–NR52)                │
             │              per-frame: gate countdown, release, pitch envelope, arpeggio,   │
             │              vibrato, PWM duty cycling, wave fade, noise drift, meters       │
             │ input_isr(): latch joypad edges                                              │
             └──────────────────────────────────────────────────────────────────────────────┘
```

The design separates **decision making** (generator, main loop) from **sound production** (driver, interrupt):

* The generator is a pure function of the project, the seed and the user's input history. It produces time-stamped `NoteEvent`s for future frames and owns a *virtual model* of the four hardware channels, so voice allocation is decided deterministically at generation time.
* The driver never makes musical decisions. It plays exactly what was scheduled, at the scheduled frame, and performs cheap per-frame modulation.
* Screen drawing can never interrupt or delay sound: the driver runs in the VBlank interrupt; a slow screen update only consumes generator look-ahead. Joypad presses are latched in the interrupt so they are not lost during slow operations (SRAM verification, full redraws).

## 2. Source layout

| Directory | Contents | Bank |
|---|---|---|
| `src/main.c` | boot, interrupt setup, main loop, profiling | 0 |
| `src/audio/` | `audio_hw.c` VBlank driver, `audio_queue.c` event queue + live wavetables, `tables.c` generated pitch / noise tables | 0 |
| `src/generative/` | `rng.c` (bank 0), `engine.c` heartbeat, emission, voice allocation, phrase players, noise texture; `agents.c` the eight agents; `motif.c` IMPRINT | auto |
| `src/harmony/` | `harmony.c` scales, chords, weighted pitch choice, harmonic memory | auto |
| `src/ecology/` | `ecology.c` the four ecological variables | auto |
| `src/timing/` | `form.c` clock, bars, seconds, scenes, glides, FOREVER | auto |
| `src/synthesis/` | `modulation.c` modulation matrix; `patches.c` wave templates/transforms, factory patch library | auto |
| `src/presets/` | `factory.c` default composition + 8 factory compositions | auto |
| `src/storage/` | `save.c` versioned redundant saves; `sram_gb.c` MBC5 SRAM access (bank 0) | auto / 0 |
| `src/interface/` | `gfx.c` text/meters/input/param-list editor, header and help panel (bank 0); `ui*.c` six pages (banked); `gfx_data.c` generated font & graphics; `help_data.c` generated help texts (`assets/help.txt`, `tools/mkhelp.py`) | 0 / auto |
| `include/` | `aether.h` project data model, `engine.h` runtime + API, `audio.h`, `gfx.h`, `ui.h`, `storage.h`, `platform.h` | — |
| `tools/` | `mktables.py` (frequency/noise tables), `mkgfx.py` (font, meters, logo), `package.sh` | — |
| `tests/` | host test-suite, simulator, emulator tests | — |

The generative core (`src/generative`, `harmony`, `ecology`, `timing`, `synthesis`, `presets`, `storage/save.c`, `audio/audio_queue.c`) is platform-independent C and is compiled unchanged for the host test-suite.

## 3. Cartridge, banking and memory

* **Mapper:** MBC5 + RAM + BATTERY (header `0x147 = 0x1B`).
* **ROM:** 128 KB (header `0x148 = 0x02`, 8 × 16 KB banks; banks 0–5 used, 6–7 free).
* **SRAM:** 32 KB (header `0x149 = 0x03`, 4 × 8 KB banks).
* **Mode:** original Game Boy (CGB flag `0x00`). No Game Boy Color features are used.

ROM banking uses GBDK-2020 auto-banking (`#pragma bank 255`, `-autobank`). Measured placement in v1.0.0:

| Bank | Used | Contents |
|---|---|---|
| 0 | 12.4 KB | runtime, main loop, **VBlank audio driver**, event queue, RNG, display & input primitives, parameter-list editor, SRAM access, page header |
| 1 | 16.0 KB | agents, wavetable / patch library |
| 2 | 16.1 KB | engine heartbeat, voice allocation, presets, PROJECT page |
| 3 | 15.3 KB | SYNTH, AGENTS, FORM pages |
| 4 | 15.5 KB | form/scenes, harmony, motifs, LIVE page and UI framework |
| 5 | 6.8 KB | ecology, modulation, storage, HARMONY page, graphics data |

Rules that make banking safe:

1. Everything the interrupt touches (driver code, frequency tables, queue, live wavetables, project data in WRAM) is in bank 0 or WRAM. The interrupt never switches banks.
2. Cross-file calls between banked modules are declared `BANKED` (GBDK trampolines save/restore the bank).
3. Constant tables are only read by code in the same source file. Banked pages pass their own strings and tables to bank-0 helpers (`put_*`, `list_*`, `ui_header`, `ui_msg`), which run while the caller's bank is still mapped.

**WRAM** (8 KB): the `Project` (915 bytes), engine runtime (`Engine`, 1.4 KB), event queue (384 bytes), live wavetables (64 bytes), the 1 KB dynamic-tile shadow (trace / waveform), UI state; ~4.1 KB static data in total, the rest is stack.

## 4. Timing system

| Level | Clock | What happens |
|---|---|---|
| Audio | VBlank, 59.73 Hz | due events start; gate countdown; vibrato, arpeggio, pitch envelope, PWM, wave fade, noise drift |
| Micro | one `engine_tick()` per generated frame | CLOCKED step clock (16-bit phase accumulator, 16th notes: `+= BPM × 73` per frame), modulation matrix every 4 frames, ecology every 16 frames, noise texture, phrase players, agents |
| Meso | bars: 16 steps (CLOCKED) or 128 frames ≈ 2.1 s (FREE) | chord changes, tonal-centre drift / return, erosion-driven harmonic simplification |
| Macro | seconds (8.8 accumulator, 15291/256 frames per second) | scene timers and transition rules, parameter glides, TIDE level, ERODE level, FOREVER evolution, elapsed clock (h:mm:ss) |

* **Look-ahead:** the generator keeps `gtime` up to 6 frames ahead of `audio_now`, generating at most 8 frames per service call; the main loop services it twice per frame (before and after the UI).
* **Counters:** `audio_now`/`gtime` are 16-bit and compared with signed differences (wrap-safe); `ticks` is 32-bit; elapsed time is seconds (16-bit, wraps hourly into an hour counter). Nothing overflows in practice (the hour counter wraps after 255 hours).
* **Overload behaviour:** if the main loop falls behind, ticks are *never skipped* (to preserve the decision sequence); events generated late are played immediately by the driver. If the queue has fewer than 8 free slots, generation pauses until the driver drains it (`st_qfull` counts this). The PROJECT ▸ STATS screen shows the measured costs.

Measured in the emulator (PyBoy, FIRST LIGHT, see `docs/TESTING.md`): VBlank driver ≈ 20 scan-lines per frame on average (13 % of a frame), worst observed ≈ 60 lines; generator ≈ 8 lines per frame on average; the generator never fell more than 1 frame behind its look-ahead.

## 5. Audio channel allocation

Eight logical agents (plus the noise-texture process and user audition) share the four DMG channels through `voice_alloc()`:

| Instrument type | Channels | Notes |
|---|---|---|
| PULSE | CH1 (sweep) and CH2 | CH1 preferred when the patch uses frequency sweep |
| WAVE | CH3 | wavetable loaded at note onset only |
| NOISE | CH4 | |

Allocation algorithm (generator side, deterministic):

1. Candidate channels = compatible channels whose **role** accepts the note (AUTO; DRONE accepts only notes ≥ 120 frames or priority ≥ 6; MELODY accepts < 240 frames; PARTICLE accepts ≤ 48 frames; OFF accepts nothing).
2. A free candidate (virtual `until` has passed) is used immediately.
3. Otherwise a sounding note may be **stolen** only if it has sounded at least 10 frames (anti-retrigger), is not protected (drones ≥ 200 frames, TIDE drones and notes on DRONE-role channels are protected; only priority 7 overrides), and the new note has higher priority (or equal priority and the old note is older than 48 frames). The lowest-priority, oldest note is chosen.
4. If nothing qualifies the request is dropped (silence is a valid outcome). Agents with a fallback instrument (INSTR 2) retry once on it.

Legato: a patch with RETRIGGER = LEGATO changes pitch without retriggering when the same agent still owns the channel. Never more than four simultaneous hardware voices exist.

## 6. Data structures

All persisted structures contain only 8-bit fields, so SDCC and gcc lay them out identically (a compile-time check asserts `sizeof(Project) == 915`).

* `Project` — name[18], seed (2), macros[6], harmony[26], ecology[11], global[12], 8 × `Instrument` (23 params + 6-char name), 4 × 16-byte wavetables, 8 × `AgentCfg` (21 params), 4 × `ModSlot` (4), 8 × `Scene` (22 params + 8-char name), 4 × `Motif` (6 + 8 × 3).
* `NoteEvent` (12 bytes) — time, duration, channel, note, instrument, volume, pan bits, aux (staged wave buffer or duty override), detune, flags.
* `Engine` (runtime) — per-agent state (`AgentState`: own RNG stream, timer, pattern/phrase buffers, motif), virtual channels, 32-entry event history, 3 phrase players, ecology, harmony state (pitch-class and interval histograms, recent pitches, centre history), modulation runtime, form state (with a *live copy* of the running scene), TIDE/ERODE levels, 4 automatic motifs, a 13 × 13 interval Markov table, statistics.

## 7. SRAM layout and save format

See `docs/BUILD.md` §Save compatibility for user-facing notes.

```
linear address = (slot × 2 + copy) × 2048         slot 0..7, copy A=0 / B=1
bank = address / 8192, offset = 0xA000 + address % 8192   (4 copies per bank)

+0   'A' 'E' 'T' 'H'      magic, written LAST (commit marker)
+4   format version       (1)
+5   generation counter   (mod 256, newer wins)
+6   payload size         (lo, hi) = 915
+8   Fletcher-16 payload  (lo, hi)
+10  Fletcher-16 of bytes 4..9 (header integrity)
+12  reserved (4)
+16  Project payload (915 bytes)
```

Saving: invalidate the target copy's magic → write payload → write header → write magic → read back and verify. The target is always the older (or invalid) copy, so the previous good version survives a power loss mid-save. Loading picks the valid copy with the newest generation. Status per slot: EMPTY (no AETHER signature — includes a new cartridge's random memory), OK, CORRUPT (signed but nothing valid), BACKUP (the newest copy is damaged and the older valid copy will be loaded). Uninitialised SRAM (random or 0xFF/0x00) never validates. One slot's damage never affects another slot. 8 slots × 2 copies × 2 KB = 32 KB.

## 8. Reproducibility

Each agent owns an independent 16-bit xorshift stream seeded from the project seed, the agent's SEED parameter and its index; global decisions (harmony, ecology-driven choices, form) use a separate stream seeded from the project seed. Engine state is fully reset by RESTART / LOAD. Given the same project, seed, software version and input history, the generated event sequence is identical — including when the main loop is serviced irregularly (verified by `t_seed_reproducibility`). The ROM driver's noise *timbre drift* uses its own small LFSR inside the interrupt; it changes only the noise colour of sounding noise notes, not any musical decision.
