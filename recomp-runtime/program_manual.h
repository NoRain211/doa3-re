#ifndef DOAXBV_RECOMP_PROGRAM_MANUAL_H
#define DOAXBV_RECOMP_PROGRAM_MANUAL_H

#include "runtime.h"

RecompFunction recomp_lookup_manual(uint32_t guest_address);

/* True while DOA3's boot routine shows the legal notice; see program_manual.c. */
bool recomp_doa3_boot_fast_forward(void);

/* Runs the adapter bound to guest_address, or stops when none is bound.
   program_forwards.c defines each rebound sub_X with it. */
void recomp_program_manual_call(uint32_t guest_address);

#ifdef RECOMP_DOA3_JP_BINDINGS
/* The address whose adapter replaces this Japan 3.1 entry point, or 0 when
   none does; program_forwards_jp.c. */
uint32_t recomp_doa3_jp_usa_address(uint32_t guest_address);

/* The guest region holding the roster tables the Japan lift relocates,
   filled from the mapped XBE in ram and disc_root/roster.txt;
   program_forwards_jp.c. */
RecompMemoryRegion recomp_doa3_roster_map(const uint8_t *ram, const char *disc_root);
/* Japan: where the lifted code keeps a table the roster moved (else guest). */
uint32_t recomp_doa3_roster_live(uint32_t guest);
#endif

/* Radio shuffle: a random playlist index other than playing whose location
   mask allows location (a location outside 0..7 allows every entry), or -1
   when there is none. random is any host random value. */
int recomp_music_shuffle_pick(const uint16_t *masks, int count, int playing,
                              int location, uint32_t random);

#endif
