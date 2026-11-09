# AETHER — Known limitations

**AETHER — by Synth Centric** · v1.0.0

This list describes what the implementation does *not* do, or does with trade-offs. Nothing listed here is presented elsewhere as a working feature.

## Verification
* **No physical hardware test.** AETHER was developed and verified in the PyBoy emulator (automated, headless) and with host-side simulations. It has not been run on an original DMG-01 or on a flash cartridge by the developer. Behaviour that depends on analogue details of real hardware (DAC pops, exact envelope/zombie-mode quirks) is unverified.
* **One emulator.** SameBoy and BGB were not available in the build environment; only PyBoy was used.
* **Demo audio** in `release/demo_audio/` is the ROM's output as rendered by PyBoy's APU emulation (48 kHz, normalised), not a recording of real hardware.

## Sound
* Four voices maximum (the hardware limit). With busy configurations some note requests are dropped by the voice allocator; this is counted (PROJECT ▸ STATS) and is part of the design.
* Stereo is discrete (left, right, both). No continuous panning.
* The wave channel has four output levels (mute/25/50/100 %); its "fade" is stepwise. It has no hardware envelope.
* Wavetable morphing is a discrete 9-step blend chosen at each note onset; wavetables are replaced only at note onsets (never crossfaded).
* Note release silences pulse/noise channels by retriggering them at volume 0 with the DAC kept powered; the wave channel is muted through its output level, and power-cycled before each wave note (needed to avoid DMG wave-RAM corruption). Small clicks at wave-note onsets may be audible on some hardware.
* Vibrato, arpeggio, pitch envelopes, PWM and noise drift are software modulation at frame rate (59.73 Hz).
* Volume changes during a note are not possible on pulse/noise channels without retriggering, so the VOLUME modulation destination affects the start volume of new notes only.

## Generation
* Exactly one agent of each kind (8 agents). Agents cannot be duplicated.
* 4 modulation slots, 4 user motifs of up to 8 notes, 32-note history, 4 automatic motifs, 8 scenes, 8 instruments, 4 wavetables per project.
* Determinism holds for identical input histories. Input timing (when a button is pressed) is part of the history.
* FOREVER mode can transpose the project's key (HARMONY ▸ ROOT) at scene changes; if the project is saved afterwards, the new key is saved.
* At a scene change, scene macro values (unless KEEP) glide over the user's current macro settings.
* CLOCKED timing is quantised to video frames: 16th notes fall on the nearest frame (±8 ms jitter at most).
* The noise channel's "pitch" is timbral and does not follow the harmony.

## Interface
* Full screen redraws (entering a page) take one to two frames; audio is unaffected (interrupt-driven, generator look-ahead), but the screen may lag slightly during dense passages.
* Names use the instrument character set (uppercase letters, digits and a few symbols).
* The LIVE trace shows the last ~13 s of generated pitches at 2-semitone vertical resolution.

## Not implemented (optional expansions in the specification)
* Link-cable synchronisation and multi-device performance.
* Advanced scene morphing beyond macro/tempo glides and staggered agent entries.
* Additional visualisation modes.
* Game Boy Color enhancements (intentionally: AETHER targets the original DMG).
