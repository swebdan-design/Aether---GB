# AETHER — Generative Engine

**AETHER — by Synth Centric**

This document describes the algorithms implemented in `src/generative`, `src/harmony`, `src/timing` and `src/synthesis/modulation.c`.

---

## 1. Agents: common behaviour

Eight logical agents are musical decision processes. They own no hardware channel; they emit note requests that the voice allocator places on one of the four DMG channels (see `ARCHITECTURE.md` §5).

Each agent runs only if it is **ENABLED** *and* its bit is set in the running scene's **AGENTS** mask. Every generated frame its timer counts down (frames in FREE mode, 16th-note steps in CLOCKED mode). When it reaches zero the agent *decides*, then the next interval is computed:

```
interval = RATE table value
         × (24 − MOTION) / 16                          MOTION macro: faster when high
         × (128 − energy_influence) / 128              high ENERGY shortens intervals
         × (384 − tide) / 256                          when TIDE is enabled (not for TIDE itself)
         + erosion / 512 × interval                    ERODE extends silences
         ± TIMING jitter (FREE: up to TIMING/16 of the interval; CLOCKED: occasional extra steps)
```

RATE table — FREE (seconds): 0.10, 0.13, 0.20, 0.27, 0.40, 0.54, 0.80, 1.07, 1.61, 2.14, 3.0, 4.0, 6.0, 8.0, 12.1, 16.1.
CLOCKED (16th-note steps): 1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64, 96, 128, 192, 255.

Whether a decision produces a note (`fire_ok`):

```
p = ACTIVITY × 16
  × (DENSITY + 6) / 14          DENSITY macro ×0.43 … ×1.5
  × (40 − SPACE) / 40           SPACE macro   ×1.0 … ×0.63
  + energy_influence / 2
  × (tide + 128) / 256          when TIDE is enabled
  + modulation[DENSITY] / 2
```

*Influence* of an ecological variable on an agent = (variable − 128) × INFLUENCE(variable) × ECO SENS / 512, i.e. −64…+64.

Durations are drawn between DUR MIN and DUR MAX (table, seconds: 0.07, 0.10, 0.13, 0.20, 0.27, 0.40, 0.54, 0.80, 1.07, 1.61, 2.14, 3.2, 4.3, 6.4, 8.6, 12.9), lengthened by STABILITY (×0.5…×1.5), shortened at high TIDE, then scaled by the instrument's LENGTH and the LENGTH modulation destination.

Pitch: agents state an *intention* (a target note), and the harmony engine chooses the actual note near it (§3). **HARMONY** (adherence 0–15) controls how strictly notes stay in the scale (15 = always in scale; lower values let chromatic colour through). Tension lowers adherence; the resolution phase raises it.

Register: LOW/HIGH, shifted by the NOTE SEL modulation destination and, when TIDE is enabled, up to ±3 semitones by the tide (high tide = higher register).

## 2. The eight agents

### ORBIT — repetition around a tonal centre
* State: a cyclic figure of LENGTH (2–8) offsets from the centre, chosen from an **INTERVALS** set (FIFTHS, TRIAD, FOURTHS, OCTAVES, STEPS, CLUSTER, WIDE, PENTA) and limited to **RADIUS** semitones. The first element is always the centre.
* Each decision plays the next element; the centre (the tonal centre nearest the middle of the register) is re-read at every cycle start, so harmonic changes move the whole figure.
* At the end of each cycle, with probability `VARIATION × 10 + mutation / 2`, one element (never the first) is replaced — a new interval from the set, or at high tension (> 180) a free chromatic offset. ERODE can turn elements into rests; variation regrows them.
* Ecology: playing the tonal centre raises STABILITY. Tension > 170 widens the pitch-choice radius.
* An IMPRINT whose SOURCE OF is ORBIT replaces the figure (its relative pitches become the figure).

### BLOOM — motif → phrase → contraction → return
* State: an origin motif of MOTIF LEN (2–6) scale-degree offsets with its own rhythm (duration per note), and a working phrase (≤ MAX LEN, 4–16).
* GROW: at the end of each pass the phrase is extended with a varied copy of the motif: transposed by −3…+3 degrees from the last note; with probability `BRANCHING × 12 (+40 when tension is high)` the copy is an inversion or a retrograde; each new note may become a rest with probability `SILENCE × 10 + SPACE × 4`. Raises ENERGY (branches raise TENSION).
* CONTRACT: when the phrase cannot grow further, half a motif length is removed from the front per pass (lowers ENERGY).
* RETURN: when contracted back to motif length, the original motif returns (probability ≈ 35 % + MEMORY influence; raises MEMORY) or the motif mutates by one degree (and possibly one duration). Then growth begins again.
* An IMPRINT whose SOURCE OF is BLOOM becomes the origin motif (semitones mapped to degrees).

### DRIFT — constrained random walk
* State: the current position in **scale degrees** relative to the centre; starts at START (−7…+7).
* Step = random in ±MAX STEP (+ positive ENERGY influence / 32 + MOTION / 6, max 7), biased by BIAS (−7…+7: the probability of an extra step in that direction), or, with probability `RETURN × 12 + stability influence`, a step back toward the centre.
* Register boundaries reflect the walk. Large steps (> 3 degrees) raise TENSION.

### SCATTER — sparse particles
* Each decision is first silenced with probability `SILENCE × 14 + SPACE × 4`.
* Pitch: uniform in the register, then harmonised with a wide radius (3).
* Duration index varied by ± DUR VAR / 4; instrument chosen among INSTR … INSTR + TIMBRES (timbre selection).
* CLUSTER: after a particle, with probability `CLUSTER × 14 + energy influence`, a burst of 1–4 further particles follows at 3–8 frame (or 1-step) spacing.
* Each particle raises ENERGY more than other agents' notes.

### TIDE — long-timescale activity
* Once per second: a triangle oscillator with PERIOD (30 s … 7.5 min) and DEPTH, mixed 3:1 with a randomly wandering target, slew-limited to ±4 per second → `tide` 0–255. When TIDE is disabled or out of scene, the tide relaxes to 128 (neutral).
* The tide scales every other agent's note probability (×0.5…×1.5) and interval (×1.5…×0.5), shortens/lengthens durations, shifts registers, pushes ENERGY (proportional to ECO SENS) and adds TENSION above 200.
* As a voice it plays protected long **drones** (DUR MAX) on the tonal centre or its fifth, with probability `DRONES × 16` per decision, and never retriggers its own drone while it still sounds. Drones raise STABILITY.

### ECHO — compositional echo
* Probability `ACTIVITY × 16 + memory influence + SPACE × 4`, scaled by DENSITY.
* Source: the most recent (or up to 5 events older) note of another agent within the last 40 s; then 3–6 of that agent's recent notes are gathered from the history in chronological order, keeping relative pitch, relative onset and duration.
* Transformations: octave displacement (DISPLACE: none / up / down / random / fifth); interval modification with probability `INTERVAL × 12` (inversion or widening by half); rhythmic displacement (delay = DELAY rate value + up to 16 frames; with mutation probability the phrase is stretched ×1.5); fragmentation (each note dropped with probability `FRAGMENT × 12 / 256`); lower intensity (volume − 3).
* Played through a phrase player on ECHO's own instrument; raises MEMORY.

### ERODE — dissolution
* Erosion level (0–240), once per second while ERODE is active: rises by (SPEED + 1) / 2 up to STRENGTH × 16. In CYCLE mode, reaching the maximum starts a regrowth phase (−6 per second to 0, then an ENERGY burst); in HOLD mode it stays. When ERODE is inactive the level decays by 8 per second.
* Effects of the level: every note request is deleted with probability erosion / 1024 (`st_eroded` counts these); all agents' intervals lengthen; harmony is simplified at levels ≥ 64 / 128 / 192 (scale reduced to chord tones + root/fifth, then chord + root, then root + fifth).
* On its own decisions (probability ACTIVITY × 16): drains ENERGY (by 4 + ECO SENS / 2) and TENSION, delays every other agent's next decision by erosion / 16 frames, and with probability `MOTIF CUT × 14` removes motif components: a rest in ORBIT's figure (never all), a rest in BLOOM's phrase, or halves a row of MEMORY's Markov table (forgetting). ERODE emits no notes.

### MEMORY — learning and recall
* Learns from everyone: every pitched note updates a 13 × 13 interval-transition table (intervals clamped to ±6 semitones; rows are halved when a count saturates).
* Library: every `18 − LEARNING` decisions it captures 4–7 recent notes into a bounded automatic motif library (4 slots, round-robin).
* Recall: with probability `RECALL × 8 + memory influence` it replays a motif chosen by recurrence weight among the user IMPRINT library (weight RECUR, +6 if the imprint's SOURCE OF is MEMORY) and its automatic library.
* Otherwise it samples its next interval from the transition table row of its last interval (FOCUS sharpens the distribution: weights squared), then harmonises the result. Raises MEMORY.

## 3. Harmonic engine

* **Scales** (12-bit pitch-class sets): Major, Natural minor, Major pentatonic, Minor pentatonic, Dorian, Phrygian, Lydian, Mixolydian, Chromatic, User set; rotated to the tonal centre (ROOT). LOWEST/HIGHEST bound every note in the piece.
* **Chord:** the triad on the current scale degree (degrees d, d+2, d+4 of the scale; diatonic triads are used for Chromatic).
* **Pitch choice** (`harm_choose`): for each candidate within ±radius of the intention and inside the register, an additive score:
  * base 34; chromatic (out-of-scale) notes −(2 × adherence + 4), forbidden at adherence 15;
  * distance to intention −2 per semitone;
  * consonance vs chord root: chord root 15, chord tones 13, otherwise the INTERVAL weight table (0–15), multiplied by CONSONANT / 8 — or, when a *dissonance event* fires (probability `DISSONANT × 10 + HARMONY macro × 4 + tension × INFL T / 64`, quartered during resolution), the inverted preference;
  * harmonic memory (MEM POWER + memory influence): REINFORCE adds the pitch-class histogram, EXPLORE subtracts it, DRIFT favours neighbours (±fifth) of what has been used rather than the used pitches themselves;
  * melodic motion from the agent's previous note: repeated notes −6 (less at high stability), leaps above a fifth −2 per semitone, plus the interval weight / 4;
  * register separation: −9 if within SEPARATE semitones of a note sounding on another channel, −5 for exact doubling.
  The score (0–63) is mapped through an exponential table (×2 every 5.5 points) and sampled.
* **Meso time** (each bar): chord change with probability `CHANGE × 3 + HARMONY × 2 + EVOLUTION + tension / 16 − stability × INFL S / 128`; during resolution or with probability stability / 2 the chord returns to I, else IV, V, vi, ii, iii, vii are weighted 27/27/20/12/10/4 %. Moving to V or vii raises TENSION, returning to I lowers it and raises STABILITY.
* **Tonal centre:** in DRIFT memory mode (or with HARMONY macro > 11) the centre can move a fifth/fourth (probability scaled down by CENTRE stability); otherwise it returns home with probability `CENTRE × 6 + stability / 8`. Modulations raise TENSION and lower MEMORY.
* **Harmonic memory:** decaying pitch-class and interval-class histograms (+24 per note, −1/32 per note), a ring of 16 recent pitches, and the last four tonal centres. The HARMONY page displays the histogram and the current scale.

## 4. Modulation matrix

Four slots, evaluated every 4 frames. Sources (−128…127): OFF, LFO (triangle, 16 rates), RND WALK, S&H, ENVELOPE (attack/decay restarted at each scene change; RATE = attack length), DRIFT (very slow smoothed random), STEP (8-step pattern hashed from the slot), SCENE (ramp across the scene's length), ENERGY, TENSION, STABILITY, MEMORY, TIDE. DEPTH −15…+15. Outputs are summed and clamped per destination:

| Destination | Effect |
|---|---|
| PITCH | transposition ±4 semitones at note onset |
| NOTE SEL | register window shift ±8 semitones |
| DENSITY | firing probability ±64/256 |
| WAVE | wavetable morph amount ±8 steps |
| DUTY | pulse duty override at note onset |
| LENGTH | note length ×0…×2 |
| NOISE | noise pitch index ±8 |
| VOLUME | initial volume ±8 |
| STEREO | stereo spread |
| MUTATION | mutation probability ±64/256 |

## 5. Form, scenes and FOREVER

See `AETHER_Manual.md` §11. Scene transitions: MANUAL, TIME (LENGTH × 15 s), CHANCE (after 30 s, every 15 s with probability `(CHANGE P × 16 + 8) / 256`; forced after 3 × LENGTH), ECOLOGY (after 30 s, when the condition CHANGE P is met — 0 E>192, 1 E<64, 2 T>192, 3 T<64, 4 S>192, 5 S<64, 6 M>192, 7 M<64; forced after 2 × LENGTH). NEXT: a scene, NEXT enabled scene, RANDOM (weighted towards scenes whose ENERGY target matches the current energy), END (graceful stop, or continue in FOREVER).

Entering a scene: KEEP fields leave the current value; macros and tempo **glide** linearly over GLIDE × 4 s; ecology targets change at once (the variables themselves relax gradually); newly active agents enter at random times within the glide; root/scale/mode take effect immediately (the next note).

**FOREVER:** sequences never end (END continues), MANUAL scenes advance after 2 × LENGTH, and each transition mutates the *running copy* of the scene (agent mask bit, key by a fifth, macro and ecology targets ±2) with probabilities scaled by EVOLUTION. The stored scenes are not modified (the project's root may be changed by a key mutation).

## 6. IMPRINT

* History: the last 32 pitched events (time, note, duration, agent, channel).
* CAPTURE (LIVE: A; FORM ▸ MOTIFS ▸ CAPTURE with an agent filter): the last 6 notes (or of one agent) → relative pitches, relative onsets (4-frame units), durations, the source agent's instrument. Raises MEMORY.
* Operations: PLAY (phrase player), MUTATE (interval nudge, swap, contour inversion, rhythm or duration change), BASE NOTE (transpose), TIME (×0.5 / ×1 / ×1.5 / ×2), THIN (remove the shortest note), FILL (passing note in the widest leap), INSTR, RECUR (recurrence weight for MEMORY), SOURCE OF (ORBIT / BLOOM use it as material; MEMORY prefers it), NOTES (manual entry: up to 8 notes of relative pitch, wait and length), CLEAR.

## 7. Noise texture process

Independent of the agents, the instrument chosen as TEXTURE (SYNTH ▸ OUTPUT) is played by a stepper: FREE steps every `5 + (15 − DENSITY)` frames, CLOCKED on every 16th. Each step reads the next bit of the 8-bit PATTERN. On-pattern steps fire with probability `DENSITY × 6 × (TEXTURE + 1) / 8 × activity`, randomly skipped with IRREGULAR × 5/256; off-pattern steps fire with that probability × IRREGULAR / 16. The *activity* (slowly changing, ACTIVITY rate) and ENERGY scale it. Texture notes have the lowest priority (1) and are always the first to give way.
