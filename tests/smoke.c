#include <stdio.h>
#include "engine.h"
#include "audio.h"
int main(void){
  preset_load(0); audio_init(); engine_init(); engine_set_playing(1);
  long ev=0;
  for (long f=0; f<60L*60*5; f++){ audio_now++; engine_service(audio_now);
     while(aq_tail!=aq_head){ NoteEvent*e=&aq[aq_tail]; if((int16_t)(e->time-audio_now)>0)break; ev++; aq_tail=(aq_tail+1)&AQ_MASK;} }
  printf("events %ld st_events %u dropped %u stolen %u eroded %u scene %u trans %u E%u T%u S%u M%u\n",ev,eng.st_events,eng.st_dropped,eng.st_stolen,eng.st_eroded,eng.form.scene,eng.form.transitions,eng.eco.v[0]>>8,eng.eco.v[1]>>8,eng.eco.v[2]>>8,eng.eco.v[3]>>8);
}
