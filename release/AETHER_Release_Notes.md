# AETHER 1.0.0 — Release Notes

**AETHER — by Synth Centric** · generative ambient instrument for the original Nintendo Game Boy (DMG-01)

Release date: 2026-11-09

## Contents

| File | Description |
|---|---|
| `AETHER_by_Synth_Centric.gb` | ROM — MBC5 + RAM + BATTERY, 128 KB ROM, 32 KB SRAM, original Game Boy mode |
| `AETHER_Manual.pdf` | manual (also `docs/AETHER_Manual.md`) |
| `AETHER_Source.zip` | complete source, assets, tools, tests and documentation to rebuild the ROM with GBDK-2020 4.3.0 |
| `demo_audio/*.flac` | 90 s of each of the nine compositions, rendered from this ROM in the PyBoy emulator (lossless FLAC, 48 kHz stereo, peak-normalised) |
| `AETHER_Release_Notes.md` | this file |

ROM SHA-256: see `SHA256SUMS`.

## What AETHER does

* Composes ambient music indefinitely with eight interacting agents (ORBIT, BLOOM, DRIFT, SCATTER, TIDE, ECHO, ERODE, MEMORY) on the Game Boy's four sound channels.
* Musical Ecology Engine (ENERGY, TENSION, STABILITY, MEMORY), harmonic engine with chord movement and harmonic memory, CLOCKED and FREE timing, 8 scenes with transition rules and glides, FOREVER mode.
* Synthesis editors: PULSE LAB, WAVE LAB (32-step wavetable drawing, 10 templates, 10 transformations, morphing, evolution), NOISE LAB with texture patterns, 4-slot modulation matrix, channel roles, 18 factory patches.
* IMPRINT motif capture and development, manual motif entry.
* Interface with its own visual language: orbit mark, lowercase type, dotted rules, page dots; a contextual two-line help panel for every parameter; B returns home to LIVE.
* LIVE performance page with a bank of six macro faders, scene slot, agent activity, ecology meters and a pitch trace.
* 8 project slots in battery SRAM with versioning, checksums and redundant copies; reproducible seeds.
* Nine compositions: FIRST LIGHT (default), PALE TRANSMISSION, STATIC MEMORY, DISSOLVING LIGHT, EMPTY ROOMS, GLASS ORBIT, NIGHT CURRENT, SOFT MACHINES, AFTERIMAGE.

## Verification summary

* Host test-suite: 20 test groups, ~100 000 checks, all passing (generative core compiled from the same sources as the ROM).
* Emulator (PyBoy 2.9, headless, sound emulated): boot and title, navigation of all pages, synthesis/agent/harmony/form/project editing, playback, IMPRINT capture, CLOCKED/FREE switching, scene change, save → power cycle → load, STOP without stuck notes, a 60-minute continuous playback, an all-agents stress test and CPU profiling — all checks passing. Details: `docs/TESTING.md`.
* **Not verified on physical hardware** and not tested in other emulators. See `docs/LIMITATIONS.md`.

## Known limitations (short)

Four hardware voices (requests are dropped when busy); discrete stereo; discrete wavetable morphing at note onsets; no link-cable features; not hardware-tested. Full list: `docs/LIMITATIONS.md`.

## Save compatibility

Save format version 1 (915-byte project payload). Slots are validated on every access; uninitialised or damaged data is never loaded.

## Credits

AETHER — concept, sound design, generative system and software by **Synth Centric**. Built with GBDK-2020.
