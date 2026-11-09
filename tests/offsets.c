/* AETHER - by Synth Centric
 * offsets.c : prints Engine/Project field offsets as laid out by SDCC
 * (no padding) so the emulator tests can read live engine state.
 * Built on the host with -fpack-struct. */
#include <stdio.h>
#include <stddef.h>
#include "engine.h"
#define O(name, expr) printf("  \"%s\": %u,\n", name, (unsigned)offsetof(Engine, expr))
#define P(name, expr) printf("  \"%s\": %u,\n", name, (unsigned)offsetof(Project, expr))
int main(void)
{
    printf("{\n");
    O("playing", playing); O("gtime", gtime); O("ticks", ticks);
    O("form.scene", form.scene); O("form.elapsed", form.elapsed); O("form.total_s", form.total_s);
    O("form.hours", form.hours); O("form.transitions", form.transitions); O("form.live", form.live); O("form.ending", form.ending);
    O("eco.v", eco.v); O("tide", tide); O("erosion", erosion); O("hs.changes", hs.changes); O("hs.root", hs.root);
    O("st_events", st_events); O("st_dropped", st_dropped); O("st_stolen", st_stolen);
    O("st_qfull", st_qfull); O("st_maxbehind", st_maxbehind); O("st_eroded", st_eroded);
    O("st_ag_ev", st_ag_ev); O("hcount", hcount);
    P("name", name); P("seed_lo", seed_lo); P("macro", macro); P("harm", harm); P("glob", glob);
    P("inst", inst); P("agent", agent); P("scene", scene); P("motif", motif);
    printf("  \"sizeof_engine\": %u,\n  \"sizeof_project\": %u,\n  \"sizeof_inst\": %u,\n  \"sizeof_agent\": %u,\n  \"sizeof_motif\": %u\n}\n",
           (unsigned)sizeof(Engine), (unsigned)sizeof(Project), (unsigned)sizeof(Instrument), (unsigned)sizeof(AgentCfg), (unsigned)sizeof(Motif));
    return 0;
}
