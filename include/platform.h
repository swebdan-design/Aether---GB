/*
 * AETHER - by Synth Centric
 * platform.h : thin portability layer so that the generative core can be
 * compiled both for the Game Boy (GBDK-2020 / SDCC) and for the host
 * (gcc) where the automated test-suite runs.
 */
#ifndef AETHER_PLATFORM_H
#define AETHER_PLATFORM_H

#include <stdint.h>

#ifdef __SDCC
  #include <gb/gb.h>
  /* BANKED / NONBANKED / CRITICAL come from gb.h */
  #define AE_CRITICAL_BEGIN __critical {
  #define AE_CRITICAL_END   }
#else
  #define BANKED
  #define NONBANKED
  #define AE_CRITICAL_BEGIN {
  #define AE_CRITICAL_END   }
  #ifndef BANKREF
    #define BANKREF(x)
    #define BANKREF_EXTERN(x)
  #endif
#endif

#endif
