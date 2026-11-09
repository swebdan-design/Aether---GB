# AETHER — Synthesis

**AETHER — by Synth Centric**

All sound in AETHER is produced live by the Game Boy's four-channel programmable sound generator. There are no samples, recordings or external sound banks. This document describes the instrument (patch) parameters, the WAVE LAB, the NOISE LAB and the modulation matrix exactly as implemented.

---

## 1. Hardware model

| Channel | Generator | Used for | AETHER control |
|---|---|---|---|
| CH1 | pulse + frequency sweep | bells, glissandi, shimmer | duty, envelope, sweep, software pitch modulation |
| CH2 | pulse | sustained tones, figures | duty, envelope, software pitch modulation |
| CH3 | 32 × 4-bit wavetable | drones, warm tones | wavetable, 4 output levels, software level fade |
| CH4 | noise (15-bit / 7-bit LFSR) | rain, wind, static, particles | clock/divider (24 perceptual steps), width, envelope, drift |

Stereo is **discrete**: each note is routed left, right or both through NR51. There is no continuous panning. Master volume (OUTPUT ▸ MASTER) sets NR50.

Pitch: MIDI note numbers. Pulse channels play C2–B7 (MIDI 36–107), the wave channel one octave lower for the same register value (MIDI 24–95). Frequency register values come from a generated equal-temperament table (`tools/mktables.py`); notes outside a channel's range are folded by octaves.

## 2. Instruments

A project holds 8 instrument slots. Each is of type PULSE, WAVE or NOISE (the type decides the channel). The SYNTH page lists the slots; A opens the editor. In every editor **A plays the instrument** (audition), NAME renames it, PRESET loads one of 18 factory patches (choose with LEFT/RIGHT, load with A).

### 2.1 PULSE LAB (CH1 / CH2)

| Parameter | Range | Effect |
|---|---|---|
| DUTY | 12.5 / 25 / 50 / 75 % | NR11/NR21 waveform |
| VOLUME | 0–15 | initial hardware envelope volume (agents' VELOCITY may override) |
| ENVELOPE | DECAY / SWELL | hardware envelope direction |
| ENV PACE | 0–7 | hardware envelope speed (0 = hold) |
| LENGTH | 1–15 | note length scale in eighths of the length requested by the agent (8 = 100 %) |
| VIB DEPTH / RATE / DELAY | 0–15 | software vibrato (triangle), depth proportional to pitch (even in cents), delay in 8-frame units |
| ARPEGGIO | OFF, OCTAVE, FIFTH, MAJOR, MINOR7, SUS, SPREAD, 2 OCT | 4-step pitch pattern applied by the driver |
| ARP SPEED | 1–15 | frames per arpeggio step |
| PITCH ENV | −12…+12 semitones | the note starts this far away and glides to its pitch |
| PENV SPD | 0–15 | glide speed (exponential approach) |
| SWEEP / SWEEP DIR / SWP STEP | 0–7 / UP,DOWN / 0–7 | CH1 hardware frequency sweep (NR10). A patch with SWEEP > 0 prefers CH1; on CH2 the sweep is ignored |
| RETRIGGER | LEGATO / ALWAYS | LEGATO: when the same agent still owns the channel, change pitch without restarting the envelope |
| RANDOM | 0–15 | random detune per note (register units) |
| OCTAVE | −2…+2 | transposition |
| PWM RATE | 0–15 | duty-cycle cycling during the note (12.5 → 25 → 50 %), a shimmering pulse-width effect |

### 2.2 WAVE instruments (CH3)

| Parameter | Range | Effect |
|---|---|---|
| WAVE A / WAVE B | 1–4 | wavetable slots |
| MORPH | 0–8 | static blend from A toward B in eighths. The blend is computed by the generator and loaded at the note onset |
| LEVEL | 25 / 50 / 100 % | maximum output level (DMG wave channel has only these levels plus mute) |
| FADE / FADE PACE | OUT, IN / 0–7 | software level envelope: steps the output level down (or up) every PACE × 8 frames |
| LENGTH, VIB…, ARP…, PITCH ENV, PENV SPD, RETRIGGER, RANDOM, OCTAVE | | as for pulse |
| EVOLVE | 0–15 | runtime wavetable evolution (see §3.3) |

The WAVE modulation destination adds to MORPH note by note.

### 2.3 NOISE LAB (CH4)

| Parameter | Range | Effect |
|---|---|---|
| WIDTH | 15 BIT / 7 BIT | 15-bit = hiss, 7-bit = metallic/periodic |
| VOLUME / ENVELOPE / ENV PACE | | hardware envelope |
| LENGTH | 1–15 | note length scale; for texture events the length grows with the square of LENGTH (a few frames at 2, about 7 s at 15) |
| PITCH | 0–23 | position in a 24-step table ordered from low rumble to bright hiss |
| SPREAD | 0–7 | random pitch spread per event |
| DRIFT / DRIFT SPD | 0–15 | the driver random-walks the noise clock during a note (wind-like movement) within ±DRIFT steps |
| DENSITY | 0–15 | texture event density |
| IRREGULAR | 0–15 | off-pattern events and random omissions |
| PATTERN | 8 bits | rhythmic pattern for the texture stepper (edit: LEFT/RIGHT choose a step, A toggles) |
| ACTIVITY | 0–15 | speed of the slow random change of texture activity |
| RETRIGGER | | as above |

**Texture process.** The instrument selected as TEXTURE in SYNTH ▸ OUTPUT is played continuously by the noise texture stepper (FREE: every 5 + (15 − DENSITY) frames; CLOCKED: every 16th note). It follows PATTERN, DENSITY, IRREGULAR and ACTIVITY, is scaled by the TEXTURE macro and by ENERGY, and always has the lowest priority. Noise instruments can also be assigned to agents (e.g. SCATTER particles).

Factory noise presets (perceptual descriptions of synthesised DMG noise, not recordings):

| Preset | Character | Settings in brief |
|---|---|---|
| RAIN | dense soft droplets | 15-bit, short decay, high pitch, density 12, irregular 10, full pattern |
| WIND | slow swells with moving colour | 15-bit, swell envelope, long, low-mid pitch, drift 5 |
| DUST | sparse tiny clicks | 15-bit, very short, very high, density 4, irregular 14 |
| STATIC | metallic crackle | 7-bit, short, drifting, density 9 |
| SPARK | rare bright pings | 7-bit, very short, very high, density 3 |

## 3. WAVE LAB

SYNTH ▸ WAVE LAB edits the project's four 32-sample, 4-bit wavetables, drawn large on screen.

* **SLOT** (LEFT/RIGHT): wavetable 1–4.
* **DRAW** (A to start): LEFT/RIGHT choose the sample, UP/DOWN change its level (0–15), A plays a wave instrument using this slot, B finishes.
* **TEMPLATE** (LEFT/RIGHT choose, A apply): SINE, TRIANGLE, SAW, SQUARE, ORGAN (harmonics 1–4), HOLLOW (odd harmonics), GLASS (1, 5, 9), REED (bright saw-like stack), PULSE 25, FORMANT.
* **TRANSFORM** (LEFT/RIGHT choose, A apply): RANDOM (random + one smoothing pass), MUTATE (four samples ±3), SMOOTH (3-tap low-pass), INVERT, REVERSE, SHIFT (phase, 4 samples), FOLD (wave-folding), CRUSH (4 levels), NORMAL (stretch to full range), SOFTEN (halve amplitude).
* **AUDITION**: plays the first wave instrument that uses the slot.

### 3.1 Safe replacement
Wave RAM can only be written while CH3 is off. The driver turns the channel off, copies 16 bytes and turns it on again **only at a wave note onset**, i.e. when the channel is being retriggered anyway. It also power-cycles CH3 before every retrigger to avoid the DMG wave-RAM corruption bug. Changing a wave therefore never glitches a sounding note — and it is never a crossfade: the new shape is heard from the next note.

### 3.2 Morphing
MORPH (and the WAVE modulation destination) produce *discrete* blends between two wavetables in 9 steps, rendered once per note by the generator (two staging buffers) and copied by the driver. This is a controlled discrete transformation, not continuous morphing.

### 3.3 Evolution
Wave instruments with EVOLVE > 0 slowly change the **live** copy of their wavetable at note onsets (probability EVOLVE × 8 + TEXTURE × 4 per note; 1–4 samples per change). Each step is a ±1 random walk that is pulled back whenever a sample is more than 3 levels from the stored wave, so a long performance transforms the timbre but never loses it. RESTART, LOAD and any WAVE LAB edit resynchronise the live copies. The stored waves are never altered by evolution.

## 4. Modulation matrix

SYNTH ▸ MODULATION: four slots, each SOURCE, DEST, DEPTH (−15…+15), RATE (0–15). The bottom line shows the four current source values. Evaluated every 4 frames with integer arithmetic; outputs to the same destination add up. Sources and destinations are listed in `GENERATIVE_ENGINE.md` §4.

Software modulation applied per frame by the driver (vibrato, arpeggio, pitch envelope, PWM, wave fade, noise drift) is part of the instrument; the matrix modulates *generation-time* parameters of new notes and the generator's behaviour.

## 5. Factory patch library

GLASS, HALO, BELL, CHIME (CH1 sweep glissando), PAD, ARPEG, MOTE, REED, SWELL (pulse); DRONE, WARM, SUB, CRYSTL (wave); RAIN, WIND, DUST, STATIC, SPARK (noise). Loading a preset replaces the slot's parameters and name; the project's wavetables are not changed.
