# AETHER

**by Synth Centric**

*generative ambient instrument* for the original Nintendo Game Boy (DMG-01)

---

AETHER is an autonomous, generative ambient synthesiser that runs on the original Game Boy and makes all of its sound with the Game Boy's own four-channel sound hardware. You design a musical system — instruments, eight musical agents, a harmonic framework, an ecology of moods and a form made of scenes — and AETHER composes inside it, indefinitely.

It is not a game and not a tracker. Press START, put the Game Boy down, and listen to music that develops over an hour.

## Highlights

* **Real DMG synthesis:** two pulse channels (duty, envelopes, CH1 sweep, vibrato, arpeggio, pitch envelopes, PWM), a 32-step wavetable channel with a D-pad **WAVE LAB** (templates, transformations, discrete morphing, slow evolution) and a noise channel with a **NOISE LAB** (RAIN, WIND, DUST, STATIC, SPARK textures).
* **Eight generative agents** — ORBIT, BLOOM, DRIFT, SCATTER, TIDE, ECHO, ERODE, MEMORY — sharing four hardware voices through an explicit priority-based voice allocator with drone protection and channel roles.
* **Musical Ecology Engine:** ENERGY, TENSION, STABILITY and MEMORY are read and written by every agent, interact, self-regulate and resolve.
* **Harmonic engine:** 9 scales + user pitch sets, chord movement, weighted context-sensitive pitch choice, interval weighting, harmonic memory (REINFORCE / EXPLORE / DRIFT).
* **Time architecture:** CLOCKED and beatless FREE timing, micro/meso/macro time levels, 8 scenes with TIME/CHANCE/ECOLOGY transitions and glides, **FOREVER** mode.
* **IMPRINT:** capture a phrase while it plays, mutate, transpose, re-time it, feed it to agents, or enter motifs by hand.
* **Pick up and play:** a mood pad on the D-pad (sparse↔dense, calm↔tense) drives six macro controls at once; A keeps a phrase, B moves the piece on; a plain-language menu and a song browser; every control explained on screen.
* **Projects:** 8 battery-SRAM slots, versioned, checksummed, stored twice; reproducible **seeds**.
* **Nine compositions:** FIRST LIGHT (default) and PALE TRANSMISSION, STATIC MEMORY, DISSOLVING LIGHT, EMPTY ROOMS, GLASS ORBIT, NIGHT CURRENT, SOFT MACHINES, AFTERIMAGE.

## Files

| Path | |
|---|---|
| `release/AETHER_by_Synth_Centric.gb` | the ROM (MBC5 + RAM + battery, 128 KB ROM, 32 KB SRAM, DMG mode) |
| `release/AETHER_Manual.pdf` · `docs/AETHER_Manual.md` | the manual |
| `release/AETHER_Release_Notes.md` | release notes |
| `release/AETHER_Source.zip` | complete source to rebuild the ROM |
| `release/demo_audio/` | 90-second recordings of each composition, rendered from the ROM in an emulator |
| `docs/` | architecture, synthesis, generative engine, ecology, controls, build, testing, limitations |

## Quick start

1. Load `AETHER_by_Synth_Centric.gb` in a Game Boy emulator with battery-save support, or on an MBC5 flash cartridge with 32 KB SRAM.
2. On the title screen press **START**: FIRST LIGHT begins.
3. Move the **D-pad**: the dot on the mood pad moves and the music follows (right denser, up tenser). **A** keeps the phrase you just heard, **B** moves to the next part.
4. **SELECT** opens the menu: *songs* to pick another piece, or *sounds / players / key / sections / fine tune / save-load* to go deeper. The bottom two lines always say what the buttons do.

## Build

Requires GBDK-2020 4.3.0, make, Python 3 (and gcc for the tests):

```bash
make
```
```bash
make test
```

See `docs/BUILD.md` for details, emulator tests and packaging.

## Status

Version 1.0.0. Verified with a host test-suite (20 test groups) and automated emulator tests in PyBoy, including a 60-minute playback and save/power-cycle/load. Not yet verified on physical DMG hardware — see `docs/TESTING.md` and `docs/LIMITATIONS.md`.

## Credits

**AETHER** — concept, sound design, generative system and software by **Synth Centric**.
Built with GBDK-2020 / SDCC. All sounds are synthesised in real time by the Game Boy; no samples or external audio are used.
