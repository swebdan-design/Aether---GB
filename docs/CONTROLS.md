# AETHER — Controls

**AETHER — by Synth Centric**

## Buttons

| Button | Everywhere | Notes |
|---|---|---|
| **D-pad UP / DOWN** | move through a list | auto-repeat when held |
| **D-pad LEFT / RIGHT** | change the selected value | auto-repeat after ~¼ s; large ranges (notes, seed, tempo, pattern) accelerate while held |
| **A** | enter / confirm / perform the row's action | in editors: plays the instrument (audition) |
| **B** | back / cancel / finish editing | on the top level of a page: go home to LIVE |
| **SELECT** | next page: LIVE → SYNTH → AGENTS → HARMONY → FORM → PROJECT → LIVE | always returns to the top of the next page |
| **START** | play / stop the composition | works on every page and screen |

No button combinations are used. All parameters can be edited while the composition plays. Presses are latched by the VBlank interrupt, so input is not lost while the instrument is busy (for example while verifying a save).

## Title screen
* **START** — enter the instrument and start playing immediately (the default composition FIRST LIGHT is ready).
* **A** — enter the instrument without playing.

## LIVE page
| Control | Action |
|---|---|
| LEFT / RIGHT | choose a fader (DENSITY, EVOLUTION, HARMONY, MOTION, TEXTURE, SPACE) or the scene slot |
| UP / DOWN | move the selected fader (0–15) / choose a scene on the scene slot |
| **A on a fader** | **CAPTURE IMPRINT** — store the last 6 notes as the next imprint (M1→M4, cycling); the help line confirms "imprint n captured k" |
| A on the scene slot | go to the chosen scene (with its glide) |

## Lists and editors
* The selected row has a ► cursor; ◂ value ▸ means LEFT/RIGHT change it. ▴ / ▾ in the left margin mean more rows above / below.
* The two-line help panel at the bottom always describes the selected control; confirmations appear on its second line.
* Rows whose value reads `a: …` (for example `a: open`, `a: save`) are actions: press A.
* **Bit rows** (AGENTS mask in scenes, noise PATTERN): LEFT/RIGHT move along the 8 positions, A toggles.
* **Text** (project, instrument, scene names): A on the NAME row starts editing; LEFT/RIGHT move, UP/DOWN change the character, A or B finishes.
* **Confirmation**: SAVE, LOAD, ERASE, FACTORY and STORE TO ask "A AGAIN" — press A a second time on the same row. Moving away cancels.

## Page map
```
LIVE      macros · scene · IMPRINT · system view
SYNTH     instruments 1-8 (PULSE LAB / WAVE / NOISE LAB) · WAVE LAB · MODULATION · OUTPUT
AGENTS    8 agents (L/R on/off, A edit) · ECOLOGY
HARMONY   key, scale, behaviour · INTERVALS · USER SET · memory view
FORM      timing, FOREVER, start/go/store scenes · SCENES (editor) · MOTIFS (IMPRINT library, NOTES)
PROJECT   name · seed · restart · slots (save/load/erase) · factory compositions · STATS · CREDITS
```
