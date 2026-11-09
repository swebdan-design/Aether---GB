# AETHER — Musical Ecology Engine

**AETHER — by Synth Centric**

The eight agents do not run as independent random sequencers. They live in a shared environment described by four variables. Every agent reads the environment when it decides, and every musical action writes back into it. The result is a feedback system whose state slowly drifts, self-regulates, and is steered by scenes and by the performer.

---

## 1. The four variables

| Variable | Meaning | Display |
|---|---|---|
| **ENERGY** (E) | overall activity | LIVE row 8, AGENTS ▸ ECOLOGY |
| **TENSION** (T) | harmonic / melodic instability | |
| **STABILITY** (S) | continuity, repetition, tonal grounding | |
| **MEMORY** (M) | influence of previously generated material | |

Each is an unsigned 8.8 fixed-point value (0.00–255.99). They are updated deterministically every 16 generated frames (≈ 3.7 times per second).

## 2. Update rule

For each variable, every update:

1. **Impulses** produced since the last update (sum, clamped to ±250) are applied through a *soft limiter*: a positive impulse is scaled by the remaining headroom to the top, a negative one by the distance to the bottom. The variables therefore approach their limits asymptotically and can never be driven hard into saturation.
2. **Relaxation** toward the **baseline**: the variable moves by 1/2^k of its distance to the baseline, with `k = 7 − DAMPING/4 − EVOLUTION/8` (min 2). Higher DAMPING or EVOLUTION = faster return.
3. **Baseline** = scene target (0–15 × 17) + (BIAS − 8) × 12, kept within 8…240 so no variable can rest at a rail.

Before that, the cross-couplings and silence rules add their impulses:

| Rule | Effect |
|---|---|
| silence (no note in the period) | E −6; after 8 silent periods S +2 |
| E > 150 | T + (E − 150) × COUPLING / 64 — sustained activity creates tension |
| T > 110 | S − (T − 110) × COUPLING / 64 — tension erodes stability |
| S > 150 | M + (S − 150) × COUPLING / 64 — repetition becomes memorable |
| S > 170 | E − (S − 170) × COUPLING / 128 — calm reduces activity |
| T > 205 | **RESOLUTION** starts: T −12 per update; harmony boosts consonance (+RESOLVE/2), agents' adherence +4, dissonance events ×¼, chords return to I. Ends when T < 120, with S +30 ("arrival") |

FREEZE holds all four variables (impulses are discarded).

## 3. What the agents write (impulses)

| Action | Impulse |
|---|---|
| any note | E + (2 + ECO SENS/4); SCATTER +3 more |
| note with consonance < 6 against the chord | T + (4 + ECO SENS/4) |
| chord tone / chord root | T −2, S +1 |
| recalled material (ECHO / MEMORY / IMPRINT playback) | M +6 |
| pitch class rarely heard | M −2 |
| ORBIT plays the tonal centre | S + (2 + ECO SENS/4) |
| BLOOM grows / branches / contracts / returns | E +4 / T +6 / E −3 / M +10 |
| DRIFT leap > 3 degrees | T +3 |
| TIDE (each second) | E ± (tide − 128) × ECO SENS / 256; T +2 at high tide; drone S +8 |
| ECHO restatement | M +8 |
| ERODE action / deletion / motif cut | E −(4 + ECO SENS/2), T −2 / E −3 / M −6 |
| MEMORY note | M +3 |
| chord change to V or vii / IV, ii, iii, vi / back to I | T +40 / T +15 / T −40, S +30 |
| tonal-centre modulation / return home | T +50, M −40 / S +40 |
| IMPRINT capture | M +20 |

## 4. What the ecology changes (influences)

Each agent sees each variable as an influence of −64…+64:
`influence = (value − 128) × INFL(variable) × ECO SENS / 512`.
(INFL E/T/S/M are set on the ECOLOGY screen; ECO SENS per agent on its page.)

| Variable | Influence on generation |
|---|---|
| ENERGY | note probability (+½ influence); intervals shortened (×(128 − infl)/128); DRIFT step size; SCATTER clustering; noise texture density |
| TENSION | probability of dissonance events in pitch choice (+ T × INFL T / 64); lower harmonic adherence; higher mutation; ORBIT widens its radius above 170 and introduces chromatic variation above 180; BLOOM branching; chord changes more likely |
| STABILITY | longer notes (×0.5…×1.5); less mutation; DRIFT returns to centre more; fewer chord changes; chords and tonal centre return home more; repeated notes tolerated |
| MEMORY | harmonic-memory weighting in pitch choice; ECHO activity; MEMORY recall probability; BLOOM returns to its origin motif more often |

The four values can also be used as modulation sources (ENERGY, TENSION, STABILITY, MEMORY) — for example MEMORY → LENGTH in AFTERIMAGE.

## 5. Scenes and the ecology

Each scene sets four ecology targets (0–15). On a scene change the baselines move immediately but the variables relax toward them at the damping rate, so the musical state *migrates* rather than jumps. The ECOLOGY transition rule can end a scene when the music itself reaches a state (e.g. "when tension is high"), and RANDOM transitions prefer scenes whose ENERGY target matches the current energy — the form follows what the music is doing.

## 6. Self-regulation and long-term change

* Equilibrium: impulses (activity) are balanced against relaxation (baseline); the soft limiter prevents runaway; silence drains energy; resolution caps tension.
* Slow change: TIDE pushes energy over minutes; ERODE cycles drain and restore activity; scene targets change the baselines; FOREVER mutates them further.

Measured behaviour (host simulation, 60 minutes per factory composition, `tests/simulate.c`) — e.g. FIRST LIGHT: E 15–205, T 17–187, S 54–222, M 68–243 with 22 scene changes; STATIC MEMORY (tension-biased): T 111–228, S 0–106. The test-suite verifies that the variables stay bounded, return toward their baselines, that tension resolution engages and releases, and that a busier agent measurably raises ENERGY and changes the generated music (`t_ecology_*`).

## 7. ECOLOGY screen (AGENTS ▸ ECOLOGY)

Live bars for E/T/S/M with values, a status line (RESOLVING TENSION / SILENCE SETTLING), and the settings: INFL E/T/S/M (0–15), BIAS E/T/S/M (−8…+7), DAMPING (0–15), COUPLING (0–15), FREEZE.
