# AETHER — Controls

**AETHER — by Synth Centric**

## Buttons

| Button | On the mood pad (home) | In the menu and editors |
|---|---|---|
| **D-pad** | shape the mood: right denser, left sparser, up tenser, down calmer | UP/DOWN move through a list, LEFT/RIGHT change a value (auto-repeat, accelerates on large ranges) |
| **A** | keep (IMPRINT) the last phrase | open / confirm / perform the row's action; in sound editors: listen |
| **B** | next part of the piece | back one level (editor top level → menu, menu → mood pad) |
| **SELECT** | open the menu | back to the mood pad |
| **START** | play / stop | play / stop |

No button combinations are used. Everything can be edited while the music plays. Presses are latched by the VBlank interrupt, so input is never lost.

## Title screen
* **START** — begin playing immediately (FIRST LIGHT).
* **A** — enter without playing.

## Menu
songs · sounds · players · key · sections · fine tune · save / load. The help lines describe each entry as you move to it; A opens it.

## Songs
UP/DOWN choose (the help lines describe the song), A loads it and starts playing on the mood pad. ♪ marks the song currently loaded.

## Lists and editors
* The selected row has a ► cursor; ◂ value ▸ means LEFT/RIGHT change it. ▴ / ▾ in the left margin mean more rows above / below.
* The two-line help panel at the bottom always describes the selected control; confirmations appear on its second line.
* Rows whose value reads `a: …` (for example `a: open`, `a: save`) are actions: press A.
* **Bit rows** (AGENTS mask in scenes, noise PATTERN): LEFT/RIGHT move along the 8 positions, A toggles.
* **Text** (project, instrument, scene names): A on the NAME row starts editing; LEFT/RIGHT move, UP/DOWN change the character, A or B finishes.
* **Confirmation**: SAVE, LOAD, ERASE, FACTORY and STORE TO ask "A AGAIN" — press A a second time on the same row. Moving away cancels.

## Page map
```
PLAY      mood pad · keep phrase · next part (home)
MENU      songs · sounds · players · key · sections · fine tune · save/load
SYNTH     instruments 1-8 (PULSE LAB / WAVE / NOISE LAB) · WAVE LAB · MODULATION · OUTPUT
AGENTS    8 agents (L/R on/off, A edit) · ECOLOGY
HARMONY   key, scale, behaviour · INTERVALS · USER SET · memory view
FORM      timing, FOREVER, start/go/store scenes · SCENES (editor) · MOTIFS (IMPRINT library, NOTES)
PROJECT   name · seed · restart · slots (save/load/erase) · factory compositions · STATS · CREDITS
```
