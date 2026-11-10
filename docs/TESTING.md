# AETHER — Testing

**AETHER — by Synth Centric** · v1.0.0

Three layers of verification were used. This document states exactly what was run and what was not.

## 1. Host test-suite (`make test`)

The platform-independent generative core is compiled with gcc from **the same source files** that go into the ROM (`src/generative`, `harmony`, `ecology`, `timing`, `synthesis`, `presets`, `storage/save.c`, `audio/audio_queue.c`). Hardware pieces are replaced by `tests/host_platform.c`; a test "player" consumes the event queue exactly like the VBlank driver.

Result: **20 test groups, ~103 000 checks, 0 failures.**

| Test | What it verifies |
|---|---|
| `t_rng_reproducible` | identical streams for identical seeds; different seeds diverge; zero seed remapped; full 65 535 period; uniform `rng_below` |
| `t_scale_quantisation` | every scale × every root quantises into the scale; scale-step movement; user pitch-class sets |
| `t_pitch_choice_in_range_and_scale` | 5 000 weighted choices stay in range and (adherence 15) in scale; low adherence allows chromatic colour; global LOWEST/HIGHEST always enforced |
| `t_consonance_preference` | consonance preference yields chord tones; dissonance + tension reduces them by more than half |
| `t_engine_ranges` | all nine compositions: notes within channel ranges, volumes, durations, pan bits; no type/channel mismatch |
| `t_agent_rules` | each agent alone produces notes in its register and scale; other agents silent; DRIFT step sizes; disabled / out-of-scene agents silent |
| `t_ecology_bounds_and_damping` | maximal impulses stay bounded; variables return toward baselines; soft saturation; tension resolution engages and releases; FREEZE |
| `t_ecology_feedback_changes_music` | same seed, different ecology bias → different music and >10 % more events; a busy SCATTER measurably raises ENERGY |
| `t_voice_allocation` | free-channel preference, anti-retrigger hold, priority stealing, drone protection, channel roles; 20 min of GLASS ORBIT with no channel/type or hold violations |
| `t_drone_protection` | TIDE drones on the wave channel are never interrupted |
| `t_event_queue_capacity` | queue full/empty arithmetic; a stalled consumer never overflows the queue; look-ahead bounded |
| `t_motifs` | capture from history reproduces the notes; 200 mutations keep the motif valid; transpose, time scale, thin, fill, clear; recurrence-weighted recall |
| `t_imprint_as_agent_source` | an imprint assigned to BLOOM becomes its origin motif |
| `t_scene_transitions` | TIME timing, NEXT chains, MANUAL hold, END stops gracefully, ECOLOGY and CHANCE rules fire, macro glides are gradual |
| `t_save_roundtrip` | save → load is byte-identical; copy alternation; neighbours untouched; 600 saves with generation wrap; Fletcher-16 reference value |
| `t_save_corruption` | random / blank SRAM never validates; damaged newest copy → BACKUP loads the previous version; header damage detected; re-save repairs; interrupted save keeps the previous version; erase |
| `t_seed_reproducibility` | 10 minutes produce identical event logs for the same seed; a 1-bit seed change differs; RESTART reproduces; irregular main-loop servicing (simulated slow UI) gives the identical decision sequence |
| `t_clocked_timing` | CLOCKED onsets lie on the 16th-note grid |
| `t_musical_configurations` | sparse melodic generation (EMPTY ROOMS within 5–60 events/min), dense activity (all agents, no queue overflow), harmonic changes, motif recall raising MEMORY, erosion only when ERODE is active, noise textures |
| `t_long_run_stability` | **2 simulated hours of every composition**: still playing, clock correct, music in the last 10 minutes, ENERGY/TENSION never pinned at saturation for >10 % of the time, ≥6 scene changes, no allocation violations |

## 2. Long-duration simulation (`tests/simulate.c`)

Deterministic host simulation of the engine at 59.73 Hz with the driver's queue consumption. **This is a simulation of the engine logic, not an emulator or hardware run.**

60 minutes per composition (default seeds):

| # | Composition | Events | per min | dropped by allocator |
|---|---|---|---|---|
| 0 | FIRST LIGHT | 5 558 | 92.6 | 1 207 |
| 1 | PALE TRANSMISSION | 899 | 15.0 | 174 |
| 2 | STATIC MEMORY | 7 056 | 117.6 | 2 107 |
| 3 | DISSOLVING LIGHT | 913 | 15.2 | 602 |
| 4 | EMPTY ROOMS | 1 332 | 22.2 | 20 |
| 5 | GLASS ORBIT | 12 221 | 203.7 | 3 905 |
| 6 | NIGHT CURRENT | 10 946 | 182.4 | 2 906 |
| 7 | SOFT MACHINES | 12 405 | 206.8 | 1 081 |
| 8 | AFTERIMAGE | 1 856 | 30.9 | 1 289 |

Each composition's event rate, channel usage and ecology ranges differ — the presets are distinct behavioural systems, not renamed copies.

A **10-hour** simulation of FIRST LIGHT completed with 57 859 events (96/min), 222 scene changes, no queue overflow, ecology ranges E 13–208, T 16–200, S 46–223, M 68–246 (never stuck at a limit).

## 3. Emulator tests (`make emutest`, `tests/emu_test.py`)

The actual ROM runs in **PyBoy 2.9** (headless, sound emulated at 48 kHz). The script drives the joypad like a user, reads the background tile map (on-screen text), engine state in WRAM (addresses from the linker `.noi` file, struct offsets generated from the headers) and the audio output.

Checks (all passing in the final run, see `build/emu/report.md` after running):

* Cartridge header: MBC5+RAM+BATTERY (0x1B), 32 KB SRAM, DMG mode; ROM size matches header.
* Title: AETHER logo drawn, **"by Synth Centric" directly beneath it**, "generative ambient instrument", no tabbed/inverted header, not playing before input.
* START → LIVE and playback; VBlank audio clock advances exactly 1/frame; events produced; audio output present.
* PLAY layout (song, part, mood pad, button legend); IMPRINT capture (A) stores a motif and confirms; the mood pad changes density (right) and tension (up); B moves to the next part.
* SELECT opens the menu; every menu item opens its page and B returns to the menu; SELECT returns to the pad; playback continues while navigating.
* Songs: the highlighted song is described; A loads it and returns to the pad, playing.
* SYNTH: instrument editor opens; **help panel explains the selected parameter**; VOLUME edit; A auditions; WAVE LAB opens; D-pad drawing changes the wavetable.
* AGENTS: enable toggle; activity edit; ECOLOGY screen.
* HARMONY: scale selection; **B on an editor's top level returns to the menu**.
* FORM: FREE ↔ CLOCKED (CLOCKED generates events); manual scene change.
* PROJECT: seed edit; save confirmation; verified save; slot OK.
* **Power cycle with the battery RAM, LOAD restores the saved project** (seed and name).
* STOP silences all channels (no stuck notes: RMS 0.000).
* **60 minutes of continuous emulated playback** of FIRST LIGHT: still playing; form clock = 3600 s; music in every 10-minute window; ≥10 scene transitions; no queue overflow; VBlank driver ≈ 16–20 scan-lines/frame on average, worst frame < 77 lines; generator never more than a few frames behind (within its 6-frame look-ahead); main loop (generator + UI + driver) below one frame on average.
* **All 8 agents at maximum activity, all macros at 15, 5 minutes**: still running, no queue overflow, generator within look-ahead.
* Demo recordings (`--demo 90`): each composition recorded from the ROM; CLOCKED pulse pieces audible in >60 % of 2-second windows.

## 4. Musical test configurations

| Required configuration | Where demonstrated |
|---|---|
| Sparse melodic generation | `t_musical_configurations` (EMPTY ROOMS), demo 04 |
| Dense agent activity | `t_musical_configurations`, emulator stress test |
| Harmonic changes | `t_musical_configurations` (DISSOLVING LIGHT ≥5 changes), simulation counts |
| Ecological interactions | `t_ecology_feedback_changes_music` |
| Motif recall | `t_musical_configurations` (AFTERIMAGE memory), `t_imprint_as_agent_source` |
| Erosion | `t_musical_configurations` (with vs without ERODE) |
| Long-form scene changes | `t_scene_transitions`, `t_long_run_stability`, emulator 60-min run |
| Noise textures | `t_musical_configurations`, demos 02 and 06 |
| Channel priority handling | `t_voice_allocation`, `t_drone_protection` |
| Seed reproducibility | `t_seed_reproducibility` |

## 5. Not tested

* **Physical hardware** (DMG-01 or flash cartridge) — not available.
* Other emulators (SameBoy, BGB) — not available in the build environment.
* Audio was judged by measurement (levels, activity, silence after STOP) and by waveform inspection of recordings, not by critical listening on hardware.
