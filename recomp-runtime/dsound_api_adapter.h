#ifndef DOA3_RECOMP_DSOUND_API_ADAPTER_H
#define DOA3_RECOMP_DSOUND_API_ADAPTER_H

#include "runtime.h"

#include <stdbool.h>

/* DOA3 (DSOUND 3936): the DirectSound entry points game code calls,
   replaced at the API level with PCM/Xbox ADPCM host playback. */
RecompFunction recomp_dsound_api_lookup_manual(uint32_t guest_address);
void recomp_dsound_api_reset(void);
/* While set, both DirectSound adapters drop audio they would hand to the
   host; buffer cursors and the guest-visible state still advance. Set for
   frames run again for rollback and frames played out of sight. The output
   pumps read it from their own threads. */
void recomp_dsound_set_output_suppressed(bool suppressed);
bool recomp_dsound_output_suppressed(void);
/* Buffers and the listener, for a frame snapshot. Restore stops host voices
   whose buffer the abandoned timeline changed. Both take the audio lock. */
size_t recomp_dsound_api_state_size(void);
void recomp_dsound_api_state_save(void *state);
void recomp_dsound_api_state_restore(const void *state);

#endif
