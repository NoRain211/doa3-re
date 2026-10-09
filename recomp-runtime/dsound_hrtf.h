#ifndef DOA3_RECOMP_DSOUND_HRTF_H
#define DOA3_RECOMP_DSOUND_HRTF_H

#include <stdint.h>

/* DSOUND 3936 full HRTF, rendered natively from the game's own table.
   See docs/doa3-audio.md for the guest routines each step follows. */
enum { RECOMP_HRTF_TAPS = 31, RECOMP_HRTF_RATE = 48000, RECOMP_HRTF_HISTORY = 128 };

typedef struct RecompHrtfFilter {
    const int8_t *left, *right; /* 32-byte ear records; NULL = silent */
    int delay;                  /* 48 kHz samples; > 0 delays the left ear */
} RecompHrtfFilter;

typedef struct RecompHrtfVoice {
    RecompHrtfFilter filter, rendered;
    float history[RECOMP_HRTF_HISTORY]; /* mono input at 48 kHz */
    uint32_t head;
    double phase;   /* source position between previous and next sample */
    float previous; /* last source sample */
} RecompHrtfVoice;

/* Listener-space direction (+x right, +y up, +z ahead) to the library's
   ratio-approximated azimuth (-180..180) and elevation (-90..90). */
void recomp_hrtf_angles(float x, float y, float z, float *azimuth, float *elevation);

/* bank: 1,111 adjacent left/right 32-byte pairs; map: [61][31] little-endian
   u16 pair indices. Stereo speaker mode: rear directions are not folded. */
RecompHrtfFilter recomp_hrtf_lookup(const uint8_t *bank, const uint8_t *map,
    float azimuth, float elevation);

/* Resamples mono 16-bit input from source_rate to 48 kHz and filters it into
   interleaved float stereo (1.0 = 32768 input units), without clipping.
   A filter change crossfades across this call's output.
   Returns frames written, at most out_frames; consumes all input. */
uint32_t recomp_hrtf_render(RecompHrtfVoice *voice, const int16_t *input,
    uint32_t input_frames, uint32_t source_rate, float *out, uint32_t out_frames);

#endif
