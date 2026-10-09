#ifndef DOAXBV_RECOMP_AUDIO_OUTPUT_H
#define DOAXBV_RECOMP_AUDIO_OUTPUT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Call on the runtime thread. Initialization is attempted once per process;
   an unavailable device or shutdown leaves guest audio timing unchanged.
   RECOMP_AUDIO_GAIN is the host master gain (0..1, default 1); zero stays muted.
   RECOMP_AUDIO_CAPTURE optionally records the mixed float WAV (first 60 s).
   Callers serialize all calls; the DirectSound adapter holds its clock lock. */
void recomp_audio_output_initialize(void);
void recomp_audio_output_shutdown(void);
void recomp_audio_output_reset_voice(uint32_t slot);
/* Nonzero while a device is open. */
int recomp_audio_output_enabled(void);

/* Copies PCM before returning; never retains a guest-memory pointer.
   Slots: 0..255. PCM: 1000..200000 Hz, mono/stereo, unsigned 8 bit, signed
   16 bit, or 32-bit IEEE float (1.0 = full scale; values beyond +/-1 allowed).
   Each submission is frame-aligned and at most 160000 bytes. A full queue
   drops the incoming chunk without waiting. A new or starved voice first
   queues 50 ms of silence as a jitter cushion, then trims its pitch by at most
   1% to hold that cushion against the device's actual consumption rate. */
void recomp_audio_output_submit(
    uint32_t slot, const uint8_t *pcm, uint32_t bytes,
    uint32_t sample_rate, uint32_t channels, uint32_t bits_per_sample,
    int32_t volume_hundredth_db);

#ifdef __cplusplus
}
#endif

#endif
