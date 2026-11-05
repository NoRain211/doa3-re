#ifndef DOAXBV_RECOMP_XAPI_TIME_ADAPTER_H
#define DOAXBV_RECOMP_XAPI_TIME_ADAPTER_H

#include "runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

uint64_t recomp_xapi_performance_counter(void);
uint64_t recomp_xapi_performance_frequency(void);
uint64_t xbox_ReadTimeStampCounter(void);
/* Opt-in fast-forward of an idle wait; all guest counter readers share it. */
bool recomp_xapi_skip_wait(uint64_t deadline_ns);
/* Optional game hook: while it returns true, waits are skipped as if unpaced. */
extern bool (*recomp_xapi_fast_forward)(void);
/* Guest time advances only through skipped waits; host pacing sleeps apart.
   Set before the guest starts. Only guest threads, which are fibers on one
   host thread, read or advance this clock, so it takes no lock. */
extern bool recomp_xapi_deterministic;
/* Guest clock reads in deterministic mode; a desync shows here first. */
extern uint64_t recomp_xapi_clock_reads;
/* 2001-11-15 00:00 UTC, DOA3's release, as a FILETIME: the fixed boot date
   and every file time in deterministic mode. */
#define RECOMP_DETERMINISTIC_EPOCH 126502560000000000ull
RecompFunction recomp_xapi_time_lookup_manual(uint32_t guest_address);

#ifdef __cplusplus
}
#endif

#endif
