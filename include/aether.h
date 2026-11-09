/*
 * AETHER - by Synth Centric
 * generative ambient instrument for the original Nintendo Game Boy (DMG-01)
 *
 * aether.h : persistent project data model and global constants.
 *
 * Every persisted structure is made exclusively of 8-bit fields so that the
 * in-memory layout is identical under SDCC (Game Boy) and gcc (host tests).
 * The whole Project structure is serialised verbatim into battery SRAM.
 */
#ifndef AETHER_H
#define AETHER_H

#include <stdint.h>
#include "platform.h"

#define AETHER_VERSION_STR "1.0.0"
#define SAVE_FORMAT_VERSION 1

/* ---------------------------------------------------------------- sizes */
#define NUM_AGENTS     8
#define NUM_INST       8
#define NUM_WAVES      4
#define NUM_SCENES     8
#define NUM_MOTIFS     4      /* persisted IMPRINT library               */
#define NUM_AUTO_MOTIFS 4     /* runtime library maintained by MEMORY    */
#define MOTIF_MAX      8      /* notes per motif                         */
#define NUM_MODS       4      /* modulation matrix slots                 */
#define NUM_MACROS     6
#define PROJ_NAME_LEN  18
#define INST_NAME_LEN  6
#define SCENE_NAME_LEN 8

/* ---------------------------------------------------- hardware channels */
#define CH_PULSE1 0   /* pulse + sweep */
#define CH_PULSE2 1   /* pulse         */
#define CH_WAVE   2   /* wavetable     */
#define CH_NOISE  3   /* noise         */
#define NUM_CH    4

/* instrument types */
#define IT_PULSE 0
#define IT_WAVE  1
#define IT_NOISE 2

/* -------------------------------------------------- instrument (patch) */
/* Parameter indices.  Some fields are re-used with a type specific
 * meaning; the aliases document that meaning.                           */
#define IP_TYPE     0   /* IT_PULSE / IT_WAVE / IT_NOISE                 */
#define IP_SHAPE    1   /* pulse: duty 0-3 | wave: slot A 0-3 | noise: width 0=15bit 1=7bit */
#define IP_VOL      2   /* pulse/noise: env start 0-15 | wave: level 1-3  */
#define IP_ENVDIR   3   /* 0 = decay, 1 = swell                          */
#define IP_ENVPACE  4   /* hw envelope pace 0-7 (wave: software fade pace) */
#define IP_LEN      5   /* note length scale 1-15 (8 = 100%)             */
#define IP_VIBD     6   /* vibrato depth 0-15 | noise: drift depth        */
#define IP_VIBR     7   /* vibrato rate 0-15  | noise: drift rate         */
#define IP_VIBDLY   8   /* vibrato delay 0-15 (x8 frames)                */
#define IP_ARP      9   /* arpeggio pattern 0-7 | noise: base pitch 0-23  */
#define IP_ARPSPD  10   /* arp speed 1-15 frames | noise: pitch spread    */
#define IP_PENV    11   /* pitch envelope start, semitones, 12 = none (0..24 => -12..+12) */
#define IP_PENVSPD 12   /* pitch envelope speed 0-15                     */
#define IP_SWPACE  13   /* ch1 sweep pace 0-7 | wave: morph target slot B */
#define IP_SWDIR   14   /* ch1 sweep dir 0 up 1 down                     */
#define IP_SWSTEP  15   /* ch1 sweep step 0-7 | wave: morph amount 0-8    */
#define IP_RETRIG  16   /* 0 legato (no retrigger on same channel), 1 always */
#define IP_RAND    17   /* random detune 0-15                             */
#define IP_OCT     18   /* octave offset 0..4 => -2..+2                   */
#define IP_MOD     19   /* pulse: duty-cycle modulation speed | wave: evolve rate | noise: activity drift */
#define IP_DENS    20   /* noise texture density 0-15                     */
#define IP_IRREG   21   /* noise texture irregularity 0-15                */
#define IP_PATTERN 22   /* noise texture rhythm pattern (8 bits)          */
#define IP_COUNT   23

#define IP_WSLOT   IP_SHAPE
#define IP_WMORPH  IP_SWPACE
#define IP_WAMT    IP_SWSTEP
#define IP_NWIDTH  IP_SHAPE
#define IP_NBASE   IP_ARP
#define IP_NSPREAD IP_ARPSPD
#define IP_NDRIFTD IP_VIBD
#define IP_NDRIFTR IP_VIBR

typedef struct {
    uint8_t p[IP_COUNT];
    char    name[INST_NAME_LEN];
} Instrument;                                  /* 29 bytes */

/* ------------------------------------------------------------- agents */
#define AG_ORBIT   0
#define AG_BLOOM   1
#define AG_DRIFT   2
#define AG_SCATTER 3
#define AG_TIDE    4
#define AG_ECHO    5
#define AG_ERODE   6
#define AG_MEMORY  7

#define AP_EN    0   /* enabled 0/1                                */
#define AP_ACT   1   /* activity probability 0-15                  */
#define AP_LO    2   /* register low  (MIDI note)                  */
#define AP_HI    3   /* register high (MIDI note)                  */
#define AP_INST  4   /* preferred instrument 0-7                   */
#define AP_INST2 5   /* fallback instrument 0-7, 8 = none           */
#define AP_DMIN  6   /* duration min (index into duration table)   */
#define AP_DMAX  7   /* duration max                               */
#define AP_MUT   8   /* mutation rate 0-15                         */
#define AP_RATE  9   /* event interval index 0-15                  */
#define AP_JIT  10   /* timing irregularity 0-15                   */
#define AP_HARM 11   /* harmonic adherence 0-15                    */
#define AP_ECO  12   /* ecological sensitivity 0-15                */
#define AP_PRI  13   /* priority 0-7                               */
#define AP_SEED 14   /* random stream seed 0-255                   */
#define AP_PAN  15   /* 0 C,1 L,2 R,3 ALT,4 RND                     */
#define AP_VOL  16   /* 0 = patch volume, 1-15 override            */
#define AP_P1   17   /* agent specific parameters                  */
#define AP_P2   18
#define AP_P3   19
#define AP_P4   20
#define AP_COUNT 21

typedef struct { uint8_t a[AP_COUNT]; } AgentCfg;

/* ------------------------------------------------------------ harmony */
#define HP_ROOT    0   /* 0-11 tonal centre                         */
#define HP_SCALE   1   /* scale index                               */
#define HP_LO      2   /* global lowest note                        */
#define HP_HI      3   /* global highest note                       */
#define HP_CONS    4   /* consonance preference 0-15                */
#define HP_DISS    5   /* dissonance probability 0-15               */
#define HP_CHANGE  6   /* harmonic change rate 0-15                 */
#define HP_CSTAB   7   /* tonal centre stability 0-15               */
#define HP_RESOL   8   /* resolution strength 0-15                  */
#define HP_REGSEP  9   /* register separation 0-12 semitones        */
#define HP_MMODE  10   /* memory mode 0 REINFORCE 1 EXPLORE 2 DRIFT  */
#define HP_MSTR   11   /* memory strength 0-15                      */
#define HP_UMASKL 12   /* user pitch-class set, bits 0-7            */
#define HP_UMASKH 13   /* user pitch-class set, bits 8-11           */
#define HP_IW     14   /* 12 interval weights 0-15                  */
#define HP_COUNT  26

#define SCALE_MAJOR    0
#define SCALE_MINOR    1
#define SCALE_MAJPENT  2
#define SCALE_MINPENT  3
#define SCALE_DORIAN   4
#define SCALE_PHRYGIAN 5
#define SCALE_LYDIAN   6
#define SCALE_MIXO     7
#define SCALE_CHROM    8
#define SCALE_USER     9
#define NUM_SCALES    10

#define MM_REINFORCE 0
#define MM_EXPLORE   1
#define MM_DRIFT     2

/* ------------------------------------------------------------ ecology */
#define EP_INF_E  0   /* influence of ENERGY on agents 0-15  */
#define EP_INF_T  1
#define EP_INF_S  2
#define EP_INF_M  3
#define EP_BIAS_E 4   /* baseline bias 0-15 (8 = neutral)    */
#define EP_BIAS_T 5
#define EP_BIAS_S 6
#define EP_BIAS_M 7
#define EP_DAMP   8   /* damping (return to baseline) 0-15   */
#define EP_COUPLE 9   /* cross-coupling between variables    */
#define EP_FREEZE 10  /* 1 = hold variables                  */
#define EP_COUNT  11

/* ------------------------------------------------------------- global */
#define GP_TMODE   0  /* 0 CLOCKED 1 FREE                           */
#define GP_TEMPO   1  /* BPM 30-200                                 */
#define GP_FOREVER 2  /* 0/1                                        */
#define GP_START   3  /* first scene                                */
#define GP_TEXINST 4  /* noise-texture instrument 0-7, 8 = off      */
#define GP_MVOL    5  /* master volume 0-7                          */
#define GP_ROLE0   6  /* channel roles (4 bytes)                    */
#define GP_SPREAD 10  /* stereo width 0-15                          */
#define GP_COUNT  12

#define ROLE_AUTO     0
#define ROLE_DRONE    1
#define ROLE_MELODY   2
#define ROLE_PARTICLE 3
#define ROLE_OFF      4

#define TM_CLOCKED 0
#define TM_FREE    1

/* -------------------------------------------------------------- macros */
#define MAC_DENSITY   0
#define MAC_EVOLUTION 1
#define MAC_HARMONY   2
#define MAC_MOTION    3
#define MAC_TEXTURE   4
#define MAC_SPACE     5

/* ---------------------------------------------------------- modulation */
#define MS_OFF     0
#define MS_LFO     1
#define MS_RWALK   2
#define MS_SH      3
#define MS_ENV     4
#define MS_DRIFT   5
#define MS_STEP    6
#define MS_SCENE   7
#define MS_ENERGY  8
#define MS_TENSION 9
#define MS_STAB   10
#define MS_MEMORY 11
#define MS_TIDE   12
#define NUM_MSRC  13

#define MD_PITCH   0
#define MD_NOTESEL 1
#define MD_DENSITY 2
#define MD_WAVE    3
#define MD_DUTY    4
#define MD_LENGTH  5
#define MD_NOISE   6
#define MD_VOLUME  7
#define MD_PAN     8
#define MD_MUTATE  9
#define NUM_MDEST 10

typedef struct {
    uint8_t src;     /* MS_*                         */
    uint8_t dest;    /* MD_*                         */
    uint8_t depth;   /* 0..30, 15 = zero             */
    uint8_t rate;    /* 0..15                        */
} ModSlot;

/* -------------------------------------------------------------- scenes */
#define SC_MASK    0   /* active agent bit mask                   */
#define SC_ROOT    1
#define SC_SCALE   2
#define SC_MAC     3   /* 6 macro values                          */
#define SC_TMODE   9
#define SC_TEMPO  10
#define SC_ENERGY 11   /* ecology targets 0-15                    */
#define SC_TENSION 12
#define SC_STAB   13
#define SC_MEM    14
#define SC_LEN    15   /* length in units of 15 s (1-60)          */
#define SC_TRANS  16   /* transition rule                         */
#define SC_TPARAM 17   /* probability / ecology condition param   */
#define SC_NEXT   18   /* 0-7 scene, 8 NEXT, 9 RANDOM, 10 END     */
#define SC_GLIDE  19   /* parameter glide seconds/4 (0-15)        */
#define SC_HMODE  20   /* harmonic memory mode                    */
#define SC_ON     21   /* scene exists / enabled                  */
#define SC_COUNT  22

#define TR_MANUAL 0
#define TR_TIME   1
#define TR_PROB   2
#define TR_ECO    3

#define NX_NEXT   8
#define NX_RANDOM 9
#define NX_END   10

typedef struct {
    uint8_t s[SC_COUNT];
    char    name[SCENE_NAME_LEN];
} Scene;                                      /* 30 bytes */

/* -------------------------------------------------------------- motifs */
typedef struct {
    int8_t  rel;    /* semitones relative to motif base             */
    uint8_t dt;     /* delay before this note, units of 4 frames    */
    uint8_t dur;    /* duration, units of 4 frames                  */
} MotifNote;

typedef struct {
    uint8_t len;      /* 0 = empty                                  */
    uint8_t base;     /* base MIDI note                             */
    uint8_t inst;     /* playback instrument                        */
    uint8_t tscale;   /* time scale: 1=x0.5 2=x1 3=x1.5 4=x2        */
    uint8_t recur;    /* recurrence weight 0-15                     */
    uint8_t source;   /* agent that sources it (0-7), 8 = none      */
    MotifNote n[MOTIF_MAX];
} Motif;                                      /* 30 bytes */

/* ------------------------------------------------------------- project */
typedef struct {
    char       name[PROJ_NAME_LEN];
    uint8_t    seed_lo, seed_hi;
    uint8_t    macro[NUM_MACROS];
    uint8_t    harm[HP_COUNT];
    uint8_t    eco[EP_COUNT];
    uint8_t    glob[GP_COUNT];
    Instrument inst[NUM_INST];
    uint8_t    wave[NUM_WAVES][16];
    AgentCfg   agent[NUM_AGENTS];
    ModSlot    mod[NUM_MODS];
    Scene      scene[NUM_SCENES];
    Motif      motif[NUM_MOTIFS];
} Project;

#define PROJECT_SIZE (PROJ_NAME_LEN + 2 + NUM_MACROS + HP_COUNT + EP_COUNT + GP_COUNT \
                      + NUM_INST*29 + NUM_WAVES*16 + NUM_AGENTS*AP_COUNT + NUM_MODS*4 \
                      + NUM_SCENES*30 + NUM_MOTIFS*30)

/* compile-time layout check (works on SDCC and gcc) */
typedef char project_size_check[(sizeof(Project) == PROJECT_SIZE) ? 1 : -1];

extern Project proj;

#endif
