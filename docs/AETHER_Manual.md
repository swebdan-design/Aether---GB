---
title: "AETHER — Manual"
subtitle: "by Synth Centric · generative ambient instrument for the original Game Boy"
---

# AETHER

**by Synth Centric**

*generative ambient instrument* · version 1.0.0

---

## 1. Introduction

AETHER is a generative ambient synthesiser for the original Nintendo Game Boy (DMG-01). It does not play back sequences. You design a small musical world — instruments, eight musical *agents*, a harmonic framework, an ecology of moods, and a form made of scenes — and AETHER composes inside it, continuously, using only the Game Boy's own four sound channels.

You can press START and listen for an hour. You can also shape every layer: draw wavetables, tune each agent's behaviour, capture a phrase you like and let it return in altered forms, and build a long composition that changes character over time.

AETHER was conceived and created by **Synth Centric**.

## 2. Artistic purpose

AETHER is meant to behave like a self-evolving sound installation rather than a loop. Its guiding ideas:

* **Coherence over randomness.** Every note is chosen by weighing a musical intention against the current scale, chord, recent harmonic memory and the other voices.
* **Evolution over repetition.** Motifs grow, contract and return; figures mutate slowly; tides of activity rise and fall over minutes; scenes change the rules.
* **Silence as material.** Density, space, erosion and silence probabilities are first-class controls.
* **Hardware authenticity.** Pulse, wave and noise channels are used for what they are; there are no samples.
* **Agency.** Six macro controls on the LIVE page reshape the music instantly; everything else remains editable while it plays.

## 3. Hardware compatibility

| | |
|---|---|
| Target | original Game Boy (DMG-01); runs in DMG mode on later models |
| Cartridge | MBC5 + RAM + battery, 128 KB ROM, 32 KB battery SRAM |
| Sound | the DMG's four channels: 2 pulse (one with sweep), 1 wavetable, 1 noise |
| Saving | 8 project slots in battery SRAM, each stored twice with checksums |

AETHER uses no Game Boy Color features. It has been verified in an emulator (PyBoy); see §20 and `LIMITATIONS.md` for what has not been verified on hardware.

## 4. Installation and loading

* **Emulator:** open `AETHER_by_Synth_Centric.gb` in an accurate Game Boy emulator with MBC5 battery-RAM support (SameBoy, BGB, mGBA, Gambatte). Keep the emulator's save file (`.sav`) next to the ROM to keep your projects.
* **Flash cartridge:** copy the ROM to a cartridge that supports MBC5 with 32 KB battery SRAM (most modern flash carts do). Make sure the cart is configured to save SRAM.
* **First start:** the title screen appears. Press **START** to begin playing the default composition *FIRST LIGHT* immediately, or **A** to enter without playing.

A brand-new cartridge contains random data in its save memory. AETHER detects this: empty slots show as EMPTY and are never loaded.

## 5. Controls

| Button | Function |
|---|---|
| D-pad UP/DOWN | move through lists · on LIVE: move the selected fader |
| D-pad LEFT/RIGHT | change values (hold to repeat; large ranges accelerate) · on LIVE: choose a fader |
| A | enter, confirm, perform an action; in editors: hear the instrument · on LIVE: IMPRINT |
| B | back one level; on the top level of a page: **home** (LIVE) |
| SELECT | next page (live → synth → agents → harmony → form → project) |
| START | play / stop, from anywhere |

**Reading the screen.** Every page has the same frame: the top line shows the orbit mark, the page name and where you are (`synth > 1 glass`), with ▶/■ for play state at the right. A dotted rule below carries six page dots (the filled one is the current page). The selected row has a ► cursor, and its value is framed by ◂ ▸ when LEFT/RIGHT can change it. ▴/▾ in the left margin mean the list continues above/below. Under the lower rule, a **two-line help panel** always explains the selected control; short confirmations appear on its second line, marked with a dot.

Destructive actions (SAVE, LOAD, ERASE, FACTORY, STORE TO) ask you to press **A again**. Full details: `CONTROLS.md`.

## 6. LIVE screen

The LIVE page is the instrument's face.

```
 o live > first light   ▶     page, project, play state
 ............... ●○○○○○       page dots
 D  major            free     key, scale · FREE or BPM
 s1 arrival         02:15     running scene · elapsed time
 ♪  ·   ·  :   ·  ·           trace: the last ~13 s of notes
 •orb  blm  drf •sct          the eight agents (big dot = playing now,
 •tid  ech  ero  mem           small dot = taking part in the scene)
 energy▬▬  tension▬▬          the ecology
 stable▬▬  memory ▬▬
 ...................
  ▮▮ ▮▮ ▮▮ ▮▮ ▮▮ ▮▮  s        six macro faders · scene slot
  ▮▮ ▮▮ ▮▮ ▮▮ ▮▮ ▮▮  2
  ▲▲ ·· ·· ·· ·· ··           selection marker
 ...................
  density        ▾ 8▴         readout of the selected control
 u/d value  a imprint         help / messages
```

**Playing LIVE:** LEFT/RIGHT choose one of the six faders or the scene slot at the far right. UP/DOWN move the selected fader (0–15) — or, on the scene slot, choose a scene; A then goes to it. On any fader, **A captures an IMPRINT**.

### Macro controls

Each macro (0–15) is mapped to real engine parameters:

| Macro | What it does |
|---|---|
| **DENSITY** | probability that agents' decisions become notes (×0.4 … ×1.5) |
| **EVOLUTION** | mutation rates; how fast the ecology moves; chord-change rate; how much FOREVER mutates the form |
| **HARMONY** | probability of dissonant choices; chord-change rate; above 11 the tonal centre may wander |
| **MOTION** | speed of agents' decisions (intervals ×1.5 … ×0.56); size of DRIFT's steps |
| **TEXTURE** | density of the noise texture; how often and how much wavetables evolve |
| **SPACE** | silence (agents fire less, SCATTER/BLOOM insert rests), more compositional echoes, wider stereo placement |

Scenes can glide the faders to new values (you will see them move); your adjustments always win until the next scene change.

### IMPRINT capture
Press **A** while a fader is selected to capture the last six notes as an IMPRINT (FORM ▸ MOTIFS M1…M4, cycling). See §12.

### Scene slot
Select the slot at the right (LEFT/RIGHT), pick a scene with UP/DOWN and press **A** to go there (the scene's glide applies).

## 7. SYNTH editor

The SYNTH page lists the eight instrument slots (name, type, and TEX for the noise texture instrument). LEFT/RIGHT on a slot plays it; A opens its editor. Below are WAVE LAB, MODULATION and OUTPUT.

* **PULSE LAB** (pulse instruments): duty, volume, envelope (decay/swell, pace), length, vibrato (depth, rate, delay), arpeggio (8 patterns, speed), pitch envelope (±12 semitones, speed), CH1 frequency sweep (pace, direction, step), retrigger (legato/always), random detune, octave, PWM rate.
* **WAVE instruments:** wave A and B, morph amount, level, fade (out/in, pace), length, vibrato, arpeggio, pitch envelope, retrigger, random, octave, EVOLVE.
* **NOISE LAB:** width (15-bit hiss / 7-bit metallic), volume, envelope, length, pitch (24 steps), spread, drift (depth, speed), density, irregular, pattern (8 steps), activity, retrigger.
* **PRESET:** load one of 18 factory patches (GLASS, HALO, BELL, CHIME, PAD, ARPEG, MOTE, REED, DRONE, WARM, SUB, CRYSTL, RAIN, WIND, DUST, STATIC, SPARK, SWELL).
* **NAME:** rename the instrument.
* **WAVE LAB:** draw the four 32-step wavetables with the D-pad, apply templates (sine, triangle, saw, square, organ, hollow, glass, reed, pulse, formant) and transformations (random, mutate, smooth, invert, reverse, shift, fold, crush, normalise, soften).
* **MODULATION:** four slots — source (LFO, random walk, sample & hold, envelope, drift, step, scene, energy, tension, stability, memory, tide), destination (pitch, note selection, density, wave, duty, length, noise, volume, stereo, mutation), depth, rate. The bottom line shows the sources moving.
* **OUTPUT:** channel roles (AUTO, DRONE, MELODY, PARTICLE, OFF) for each channel, the TEXTURE instrument (a noise instrument played continuously, or none), master volume, stereo spread.

Every parameter is described in `SYNTHESIS.md`.

## 8. AGENTS editor

The AGENTS page shows the eight agents with their on/off box (LEFT/RIGHT toggles), activity dot and instrument. A opens an agent; the last row opens ECOLOGY.

Common parameters for every agent:

| Parameter | Meaning |
|---|---|
| ENABLED | the agent may act (the scene must include it too) |
| ACTIVITY | probability that a decision becomes a note |
| LOW / HIGH | register |
| INSTR / INSTR 2 | preferred instrument and fallback (used when the first cannot get a channel) |
| DUR MIN / DUR MAX | note-length range (shown in seconds) |
| RATE | time between decisions (seconds in FREE, steps in CLOCKED) |
| TIMING | irregularity of that timing |
| MUTATION | how readily its material changes |
| HARMONY | adherence to the scale (15 = strict) |
| ECO SENS | sensitivity to — and influence on — the ecology |
| PRIORITY | 0–7; higher priority may take a channel from lower |
| STEREO | centre, left, right, alternate, random |
| VELOCITY | start volume (PATCH = the instrument's volume) |
| SEED | the agent's own random stream |

Agent-specific parameters:

| Agent | Parameters |
|---|---|
| ORBIT | RADIUS, INTERVALS (fifths, triad, fourths, octaves, steps, cluster, wide, penta), VARIATION, LENGTH |
| BLOOM | MOTIF LEN, BRANCHING, SILENCE, MAX LEN |
| DRIFT | MAX STEP, BIAS, RETURN, START |
| SCATTER | CLUSTER, SILENCE, DUR VAR, TIMBRES |
| TIDE | PERIOD, DEPTH, DRONES |
| ECHO | DELAY, DISPLACE, FRAGMENT, INTERVAL |
| ERODE | STRENGTH, SPEED, MOTIF CUT, MODE (cycle/hold) |
| MEMORY | RECALL, FOCUS, LEARNING |

How each agent thinks is explained in `GENERATIVE_ENGINE.md` §2. In short:

* **ORBIT** repeats a small figure around the tonal centre, changing one note now and then.
* **BLOOM** grows a motif into a longer phrase, contracts it and returns to it — or lets it mutate.
* **DRIFT** wanders step by step through the scale, drawn back toward the centre.
* **SCATTER** throws sparse particles and little clusters across a high register.
* **TIDE** is the slow breath of the piece: a rise and fall of activity over minutes, and long drones.
* **ECHO** restates what another agent just played — later, softer, displaced, fragmented.
* **ERODE** dissolves: deletes notes, opens silences, cuts holes in motifs, simplifies harmony, then lets things grow back.
* **MEMORY** learns the intervals everyone plays and recalls remembered phrases, including your IMPRINTs.

## 9. HARMONY editor

| Parameter | Meaning |
|---|---|
| ROOT, SCALE | tonal centre and pitch collection: major, minor, major/minor pentatonic, dorian, phrygian, lydian, mixolydian, chromatic, user set |
| LOWEST / HIGHEST | global register limits for every note |
| CONSONANT | preference for consonance with the current chord |
| DISSONANT | probability of deliberately dissonant choices |
| CHANGE | how often the chord moves (I, IV, V, vi, ii, iii, vii) |
| CENTRE | tonal-centre stability (resists modulation, returns home) |
| RESOLVE | how strongly tension resolves toward consonance |
| SEPARATE | keep voices this many semitones apart |
| MEM MODE | REINFORCE (favour familiar pitches), EXPLORE (favour new ones), DRIFT (move gradually away, letting the centre wander by fifths) |
| MEM POWER | strength of the harmonic memory |
| INTERVALS | 12 weights (unison … major 7th) used for consonance and melodic intervals |
| USER SET | your own 12-note collection (toggle pitch classes) |

The bottom of the page shows the current key, the chord (roman numeral), RES while tension resolves, a histogram of recently used pitch classes and the active scale (inverted letters).

## 10. Musical Ecology Engine

Four variables describe the state of the music: **ENERGY**, **TENSION**, **STABILITY**, **MEMORY**. Agents read them when they decide and write to them with every action: scattering particles raises energy, dissonance raises tension, returning to the centre raises stability, recalling material raises memory, erosion and silence lower energy. The variables interact (sustained energy creates tension; tension erodes stability; stability becomes memory), are pulled back toward the scene's targets, and can never be stuck at their limits. When tension becomes excessive, a **resolution** phase steers the harmony back to consonance.

AGENTS ▸ ECOLOGY shows the four variables live and lets you set their influence, bias, damping, coupling and freeze them. Full rules: `MUSICAL_ECOLOGY.md`.

## 11. FORM and Scenes

**FORM page:**

| Row | Meaning |
|---|---|
| TIMING | CLOCKED (events on a 16th-note grid at TEMPO) or FREE (beatless; each agent keeps its own irregular time) |
| TEMPO | 30–200 BPM |
| FOREVER | compose indefinitely, evolving the scenes (see below) |
| START AT | scene used when the composition starts |
| GO TO | A = jump to the chosen scene now |
| STORE TO | A (twice) = save the current state (active agents, key, macros, timing, current ecology) into a scene |
| SCENES | the scene list and editor |
| MOTIFS | the IMPRINT library |

The status lines show the running scene, its elapsed / planned time, the transition rule and the number of scene changes.

**Scenes** are behavioural states, not note sequences. Each of the 8 scenes stores: NAME, ENABLED, AGENTS (which agents take part), ROOT, SCALE, the six macros, TIMING, TEMPO (each may be KEEP), the four ecology targets, LENGTH, CHANGE BY, CHANGE P, NEXT, GLIDE and HARM MODE.

| CHANGE BY | The scene ends… |
|---|---|
| MANUAL | only when you choose another scene (or after 2 × LENGTH in FOREVER) |
| TIME | after LENGTH |
| CHANCE | after 30 s, with probability CHANGE P (0–15) every 15 s; at the latest after 3 × LENGTH |
| ECOLOGY | when the condition CHANGE P is reached (0 energy high, 1 energy low, 2 tension high, 3 tension low, 4 stability high, 5 stability low, 6 memory high, 7 memory low) after 30 s; at the latest after 2 × LENGTH |

NEXT: a scene (S1–S8), NEXT (the next enabled scene), RANDOM (an enabled scene, chosen with preference for scenes whose energy matches the current energy), END (stop gently — or continue in FOREVER).

Transitions are controlled: macros and tempo **glide** over GLIDE × 4 seconds, the ecology migrates toward the new targets, newly active agents enter at staggered moments; key and scale change at the next note. Wavetables and hardware registers change at note onsets, never as crossfades.

**FOREVER mode** keeps the form alive indefinitely: END continues, MANUAL scenes advance, and each transition slightly mutates the running scene (an agent joins or leaves, the key moves by a fifth, macro and ecology targets shift), more with higher EVOLUTION. It does not guarantee mathematically non-repeating music, but it avoids the impression of a short loop.

## 12. IMPRINT

IMPRINT preserves material you like while the music plays — without stopping anything.

* **Capture:** on LIVE press **A** (last 6 notes of any agent), or in FORM ▸ MOTIFS ▸ M*n* choose FROM (an agent or ANY) and CAPTURE. Relative pitches, onsets and durations are stored with the playing instrument.
* **Motif editor** (FORM ▸ MOTIFS ▸ M1–M4): PLAY, MUTATE, BASE NOTE (transpose), TIME (×0.5/×1/×1.5/×2), THIN (remove a note), FILL (insert a passing note), INSTR, RECUR (how likely MEMORY recalls it), SOURCE OF (make it the material of ORBIT or BLOOM, or MEMORY's favourite), CLEAR.
* **NOTES:** manual entry — set LEN (1–8), then per note the relative pitch (REL, ±24), the WAIT before it and its LEN (both in 4-frame units ≈ 1/15 s). A moves between columns; changing a pitch plays it.

Manually entered motifs behave exactly like captured ones. Imprints are saved with the project.

## 13. PROJECT storage

| Row | Function |
|---|---|
| NAME | rename the project |
| SEED | the project's random seed (LEFT/RIGHT; A = new random seed) |
| RESTART | restart the composition from the seed |
| SLOT | 1–8, with status (EMPTY, OK, CORRUPT, BACKUP) and stored name |
| SAVE / LOAD / ERASE | with confirmation |
| FACTORY | load one of the nine compositions (with confirmation) |
| STATS | events, dropped/stolen/eroded notes, CPU load in scan-lines, elapsed time |
| CREDITS | |

A project contains everything: name, seed, macros, harmony, ecology settings, global settings and channel roles, 8 instruments, 4 wavetables, 8 agents, modulation, 8 scenes and 4 IMPRINT motifs.

Saving is verified: each slot keeps two copies; a save always overwrites the older one and is read back. If the newest copy is ever damaged (for example power lost during a save), the slot shows BACKUP and the previous version loads. A corrupt slot never affects the others.

## 14. Reproducible Seeds

Every project has a 16-bit seed. Each agent draws from its own deterministic random stream derived from the project seed and its SEED parameter. Given the same project, the same seed, the same version of AETHER and the same button presses at the same moments, AETHER makes exactly the same musical decisions. RESTART (PROJECT page) returns to the beginning of that sequence. Change the seed to hear another performance of the same composition.

## 15. Factory presets

| # | Composition | Character | Key / time |
|---|---|---|---|
| 0 | **FIRST LIGHT** (default) | balanced introduction in five scenes: ARRIVAL, EMERGE, EXPAND, DISSOLVE, RETURN | D major, FREE |
| 1 | **PALE TRANSMISSION** | ethereal wave drones, distant high bells and their echoes, wind texture | E lydian, FREE |
| 2 | **STATIC MEMORY** | unstable drifting tones, electrical noise particles, strong memory and erosion | C# phrygian, FREE |
| 3 | **DISSOLVING LIGHT** | slowly changing consonant harmony, swelling pads, gentle erosion | G major, FREE |
| 4 | **EMPTY ROOMS** | sparse melodic fragments, long silences, rooms of echoes, dust | A minor, FREE |
| 5 | **GLASS ORBIT** | bright repeating figures with gradual mutation, chime glissandi | C major pentatonic, 96 BPM |
| 6 | **NIGHT CURRENT** | low pulses, sub drones, unpredictable rhythmic rain particles | D dorian, 68 BPM |
| 7 | **SOFT MACHINES** | interlocking minimalist cycles of 5, 6 and 7 that phase against each other | F mixolydian, 108 BPM |
| 8 | **AFTERIMAGE** | a remembered phrase recurs, transforms and erodes | B minor, FREE |

Each composition has its own instruments, agent settings, harmonic behaviour, ecology and 3–5 scenes. Demo recordings made from the ROM are in `release/demo_audio/`.

## 16. Creating your first composition

1. PROJECT ▸ FACTORY: choose FIRST LIGHT, press A twice. Rename it (NAME).
2. Press START. Listen for a minute.
3. LIVE: select the DENSITY fader and lower it to 4 (DOWN), then raise SPACE to 12 — the music opens up. Raise MOTION — it quickens.
4. AGENTS: disable SCATTER (LEFT on its row). Open ORBIT, set INTERVALS to TRIAD and RADIUS to 7.
5. HARMONY: change SCALE to DORIAN.
6. FORM ▸ STORE TO S1, press A twice: your current sound is now scene 1.
7. PROJECT ▸ SAVE (A, A). Done.

### Tutorial A — Create a slowly evolving drone
1. SYNTH ▸ WAVE LAB: SLOT 1, TEMPLATE ORGAN (A). SLOT 2, TEMPLATE HOLLOW (A).
2. SYNTH ▸ instrument 5 (DRONE): WAVE A 1, WAVE B 2, MORPH 2, LEVEL 100 %, LENGTH 15, EVOLVE 6.
3. SYNTH ▸ MODULATION slot 1: SOURCE LFO, DEST WAVE, DEPTH +12, RATE 1 — each new drone note has a different blend.
4. AGENTS ▸ TIDE: INSTR 5, LOW C2 / HIGH G3, DRONES 15, DUR MAX 15, PERIOD 4:00, DEPTH 12.
5. Disable the other agents, or keep ORBIT very low (ACTIVITY 3).
6. SYNTH ▸ OUTPUT: CH3 WAVE role DRONE.
7. Raise TEXTURE for more wavetable evolution. Listen as the drone slowly changes colour while the tide breathes.

### Tutorial B — Create an ethereal generative melody
1. AGENTS ▸ BLOOM: ENABLED, INSTR HALO or GLASS, LOW G4 / HIGH D6, MOTIF LEN 3, BRANCHING 6, SILENCE 4, MAX LEN 12, DUR MIN/MAX 0.5–1.6 s, STEREO ALTERN.
2. AGENTS ▸ ECHO: ENABLED, DELAY 1.6 s, DISPLACE OCT UP, FRAGMENT 6.
3. HARMONY: SCALE LYDIAN, CONSONANT 12, DISSONANT 1, MEM MODE REINFORCE.
4. LIVE: DENSITY 6, SPACE 11, EVOLUTION 6.
5. When a phrase appears that you like, press A on LIVE to IMPRINT it; set its SOURCE OF to BLOOM (FORM ▸ MOTIFS).

### Tutorial C — Create a dark, unstable soundscape
1. HARMONY: ROOT C#, SCALE PHRYGIAN, CONSONANT 5, DISSONANT 8, CENTRE 4, MEM MODE EXPLORE.
2. ECOLOGY: BIAS T +4, COUPLING 12.
3. SYNTH ▸ OUTPUT: TEXTURE = an instrument loaded with STATIC; in NOISE LAB raise DRIFT to 6.
4. AGENTS: DRIFT with MAX STEP 4, TIMING 12, HARMONY 8; SCATTER with a noise instrument (SPARK), CLUSTER 11; ERODE enabled, STRENGTH 9, SPEED 6.
5. MODULATION: S&H → DUTY, RND WALK → PITCH (small depth), TENSION → NOISE.
6. LIVE: HARMONY 10, TEXTURE 11. Watch tension rise, resolve, and rise again.

### Tutorial D — Build a 60-minute autonomous composition
1. Prepare five scenes (FORM ▸ SCENES), e.g. ARRIVAL, EMERGENCE, EXPANSION, DISSOLUTION, RETURN. For each, set AGENTS, macros, ecology targets and LENGTH (e.g. 8:00, 12:00, 15:00, 10:00, 15:00 = 60 min).
2. CHANGE BY TIME for all, NEXT = NEXT; GLIDE 20–40 s.
3. Make DISSOLUTION include ERODE and give it high SPACE; give RETURN high MEMORY and ECHO/MEMORY agents, so earlier motifs reappear in altered form.
4. For a finite piece: set the last scene's NEXT to END and FOREVER OFF — after the last scene AETHER fades and stops. For an endless one: FOREVER ON.
5. Tip: build each scene by playing, adjusting on LIVE, then FORM ▸ STORE TO.
6. SAVE. Press START and leave the Game Boy running.

### Tutorial E — Capture and develop a musical motif
1. While music plays, press A on LIVE when you hear a phrase you like: "imprint 1 captured 6".
2. FORM ▸ MOTIFS ▸ M1: PLAY to hear it (while the composition runs).
3. MUTATE a few times, adjust TIME to ×1.5, THIN once.
4. RECUR 12: MEMORY will now recall it often. SOURCE OF BLOOM: BLOOM grows phrases from it.
5. NOTES: refine single notes by hand.
6. Observe how ECHO and MEMORY return it in altered forms; ERODE will slowly cut holes in it.

## 17. Advanced generative techniques

* **Channel roles as orchestration.** Reserve CH3 for drones (DRONE) and CH1 for particles (PARTICLE) to keep a foundation that is never interrupted.
* **Priorities.** Give the agent that carries the identity of the piece (often ORBIT or BLOOM) priority 5–6; particles 1–2.
* **Ecology as form.** Use CHANGE BY ECOLOGY: e.g. leave a dense scene when tension is high (condition 2), so the form reacts to the music.
* **Phasing.** In CLOCKED mode give ORBIT, BLOOM and MEMORY different cycle lengths (ORBIT LENGTH 5, BLOOM MOTIF LEN 3 with MAX LEN 6, …) for Reich-like phasing (see SOFT MACHINES).
* **Seeds as performances.** Keep a composition and audition different seeds; note the ones you like.
* **Modulating the form.** SCENE source → DENSITY makes every scene build up; ENVELOPE → STEREO opens the stereo field after each scene change.
* **Memory modes per scene.** Use EXPLORE in development scenes and REINFORCE in returns.

## 18. Recording audio from a Game Boy

* Use the headphone jack (stereo). The DMG speaker is mono and filtered.
* Set the Game Boy volume wheel around ¾ and adjust recording gain for peaks around −6 dBFS; AETHER's OUTPUT ▸ MASTER sets the internal mix level.
* Use a ground-loop isolator if you hear hum when the Game Boy is powered from mains.
* The DMG output has a DC offset and some hiss; a gentle high-pass (20–30 Hz) helps.
* For long recordings use the AC adapter or fresh batteries; AETHER does not stop by itself unless a composition ends with END.
* In emulators, use the emulator's WAV/audio recording function.

## 19. Troubleshooting

| Symptom | Remedy |
|---|---|
| No sound | START pressed? Check OUTPUT ▸ MASTER and channel roles (OFF silences a channel). Check that agents are ENABLED *and* part of the current scene (LIVE agent letters). |
| Too sparse / silent stretches | raise DENSITY, lower SPACE; check ERODE; scenes may be designed sparse (EMPTY ROOMS). |
| Too busy | lower DENSITY or MOTION; raise SPACE; reduce ACTIVITY of SCATTER/ECHO. |
| A drone is never interrupted | that is protection: long notes and DRONE-role channels are protected. |
| Notes are missing | four channels only: low-priority requests are dropped when channels are busy (STATS shows DROPPED). Raise the agent's PRIORITY or use INSTR 2. |
| The key changes by itself | FOREVER mutation, MEM MODE DRIFT or HARMONY macro above 11. |
| Slot shows CORRUPT | an AETHER save in it was damaged; save into it again to repair it. Other slots are unaffected. (A new cartridge's random memory simply shows EMPTY.) |
| Slot shows BACKUP | the latest save was damaged; LOAD will load the previous version. Save again. |
| Saves lost after power off | the cartridge/emulator must support MBC5 battery SRAM (32 KB) and keep its save file. |

## 20. Technical specifications

| | |
|---|---|
| Platform | Nintendo Game Boy DMG-01 (DMG mode) |
| Cartridge | MBC5 + RAM + BATTERY, 128 KB ROM, 32 KB SRAM |
| Language / toolchain | C (SDCC via GBDK-2020 4.3.0) |
| Audio clock | VBlank interrupt, 59.73 Hz; look-ahead generator 6 frames |
| Voices | 4 hardware channels shared by 8 agents + noise texture |
| Agents | ORBIT, BLOOM, DRIFT, SCATTER, TIDE, ECHO, ERODE, MEMORY |
| Ecology | 4 variables, 8.8 fixed point, updated every 16 frames |
| Harmony | 9 scales + user set, chord progression, weighted pitch choice, harmonic memory |
| Modulation | 4 slots, 13 sources, 10 destinations |
| Scenes | 8 per project, 4 transition rules, glides, FOREVER |
| Instruments | 8 per project, 18 factory patches, 4 wavetables, 10 templates, 10 transforms |
| IMPRINT | 4 saved motifs (8 notes) + 4 automatic, 32-note history |
| Storage | 8 slots × 2 copies, versioned, Fletcher-16 checksums |
| CPU (measured, emulator) | driver ≈ 16–20 scan-lines/frame (≈ 12 %), generator ≈ 8–17 lines/frame average, UI ≈ 60 lines/frame on LIVE |
| Verified | PyBoy emulator: boot, navigation, editing, playback, IMPRINT, save/power-cycle/load, 60-minute playback |
| Not verified | physical DMG hardware, other emulators |

Architecture and algorithms: `ARCHITECTURE.md`, `GENERATIVE_ENGINE.md`, `MUSICAL_ECOLOGY.md`, `SYNTHESIS.md`. Tests: `TESTING.md`.

## 21. Credits

**AETHER** — concept, sound design, composition system and software by **Synth Centric**.

Built with GBDK-2020 (SDCC). Font, graphics and all sounds are original and generated within the ROM.

*AETHER — by Synth Centric*
