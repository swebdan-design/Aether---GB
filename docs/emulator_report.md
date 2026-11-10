# AETHER emulator test report

Emulator: PyBoy 2.x (headless, sound emulated at 48000 Hz). ROM: `AETHER_by_Synth_Centric.gb`.

Result: **59 / 59 checks passed**

| check | result | detail |
|---|---|---|
| cartridge header: MBC5+RAM+BATTERY, 32KB SRAM, DMG mode | PASS | type=1B rom=02 ram=03 cgb=00 |
| ROM size matches header | PASS | 131072 |
| title: logo tiles drawn (AETHER) | PASS |  |
| title: 'by Synth Centric' directly beneath the logo | PASS |   by Synth Centric   |
| title: no inverted / tabbed header | PASS |  |
| title: 'generative ambient instrument' | PASS |  |
| title: not playing before user input | PASS |  |
| START on title -> PLAY (mood pad) | PASS | ^ AETHER           ~ |
| playback started | PASS |  |
| VBlank audio clock advances 1/frame | PASS | 1800 |
| generative events produced (30 s) | PASS | 30 |
| audio output from DMG sound hardware | PASS | rms=0.91 |
| PLAY shows song, part, mood pad and button legend | PASS | A KEEP   B NEXT PART |
| IMPRINT capture (LIVE, A) stores a motif | PASS | len=3 msg=~IMPRINT 1 CAPTURED  |
| IMPRINT confirmation shown | PASS | ~IMPRINT 1 CAPTURED  |
| mood pad: right = denser, up = more tension | PASS | density 3->5 harmony 3->6 |
| B on PLAY moves to the next part | PASS | ~NEXT: EMERGE        |
| SELECT opens the menu | PASS |  ~ SONGS             |
| every menu item opens its page; B returns to the menu | PASS |  |
| SELECT from the menu returns to the mood pad | PASS |  |
| playback continues while navigating | PASS |  |
| songs: description of the highlighted song | PASS | UNSTABLE TONES AND   |
| songs: A loads the song and returns to the pad, playing | PASS |  STATIC MEMORY       |
| instrument editor opens (PULSE LAB) | PASS |  TYPE      PULSE     |
| help panel explains the selected parameter | PASS | START LOUDNESS       |
| PULSE LAB: VOLUME edit changes instrument 1 | PASS | 7->6 |
| A auditions the edited instrument | PASS | 0-15                 |
| WAVE LAB opens | PASS | ^ SOUNDS > WAVE LAB~ |
| WAVE LAB: D-pad draws sample 0 of wave 1 | PASS | 8->5 |
| AGENTS: ORBIT enable toggles | PASS |  |
| AGENTS: ORBIT activity editable | PASS | 6->5 |
| ECOLOGY screen shows the four variables | PASS |  ENERGY   ~~~~~~  94 |
| HARMONY: scale selection | PASS |  ROOT      C#        |
| B on an editor's top level returns to the menu | PASS | ^ MENU             ~ |
| FORM: FIRST LIGHT runs in FREE time | PASS |  |
| FORM: switch to CLOCKED | PASS |  |
| CLOCKED mode generates events | PASS | 5 |
| FORM: back to FREE | PASS |  |
| FORM: manual scene change | PASS | scene=2 |
| PROJECT: seed editable | PASS | 3900->3905 |
| save asks for confirmation | PASS | ~A AGAIN: SAVE SLOT  |
| save verified | PASS | ~SAVED + VERIFIED    |
| slot 1 reports OK | PASS |  SLOT 1 OK           |
| power cycle + LOAD restores the saved project | PASS | boot=24301 loaded=3905 saved=3905 |
| loaded name matches | PASS |  |
| playback after load | PASS |  |
| STOP silences all channels (no stuck notes) | PASS | rms=0.000 |
| extended playback still running after 60 min | PASS |  |
| form clock tracks emulated time | PASS | 1h 0s |
| music continues throughout (events every 10-min window) | PASS | 0 |
| long-form development (scene transitions) | PASS | 22 |
| no event queue overflow | PASS | 0 |
| main loop keeps up: generator + UI + driver < 1 frame on average | PASS | gen 27.2 + ui 32.1 + isr 18.7 lines of 154 |
| VBlank driver: average cost < 15% of a frame | PASS | avg 18.7 lines of 154 |
| VBlank driver: worst frame < 50% of a frame | PASS | max 55, p99 41 |
| generator max lag within look-ahead | PASS | 0 |
| 8 agents at max activity for 5 min: still running | PASS |  |
| 8 agents: no queue overflow | PASS | 0 |
| 8 agents: generator keeps within look-ahead | PASS | 0 |

## Extended playback (60 emulated minutes, FIRST LIGHT)

```
{
 "minutes": 60,
 "events": 5656,
 "transitions": 23,
 "gen_lines_max": 255,
 "ui_lines_max": 255,
 "isr_lines_max": 57,
 "isr_lines_avg": 18.7,
 "isr_lines_p99": 41,
 "gen_lines_avg": 27.2,
 "gen_lines_p99": 134,
 "ui_lines_avg": 32.1,
 "max_behind": 0,
 "dropped": 1357,
 "stolen": 482,
 "windows": [
  {
   "minute": 10,
   "events": 1045,
   "scene": 3,
   "transitions": 3,
   "E": 86,
   "T": 85,
   "S": 70,
   "M": 122
  },
  {
   "minute": 20,
   "events": 2016,
   "scene": 2,
   "transitions": 7,
   "E": 186,
   "T": 132,
   "S": 98,
   "M": 145
  },
  {
   "minute": 30,
   "events": 2726,
   "scene": 1,
   "transitions": 11,
   "E": 99,
   "T": 118,
   "S": 134,
   "M": 135
  },
  {
   "minute": 40,
   "events": 3794,
   "scene": 4,
   "transitions": 14,
   "E": 68,
   "T": 20,
   "S": 205,
   "M": 244
  },
  {
   "minute": 50,
   "events": 4760,
   "scene": 3,
   "transitions": 18,
   "E": 62,
   "T": 103,
   "S": 90,
   "M": 108
  },
  {
   "minute": 60,
   "events": 5656,
   "scene": 2,
   "transitions": 22,
   "E": 135,
   "T": 134,
   "S": 95,
   "M": 175
  }
 ]
}
```

## Dense stress (all 8 agents, all macros 15, 5 emulated minutes)

```
{
 "gen_lines_max": 255,
 "ui_lines_max": 254,
 "isr_lines_max": 56,
 "max_behind": 0,
 "events": 905,
 "dropped": 525
}
```
