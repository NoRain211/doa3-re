#ifndef DOA3_RECOMP_DSOUND_API_ADAPTER_H
#define DOA3_RECOMP_DSOUND_API_ADAPTER_H

#include "runtime.h"

/* DOA3 (DSOUND 3936): the DirectSound entry points game code calls,
   replaced at the API level with PCM/Xbox ADPCM host playback. */
RecompFunction recomp_dsound_api_lookup_manual(uint32_t guest_address);
void recomp_dsound_api_reset(void);

#endif