#ifndef DOA3_RECOMP_DSOUND_REVERB_H
#define DOA3_RECOMP_DSOUND_REVERB_H

#include <stdint.h>

/* DSOUND I3DL2 listener reverb, rendered natively at 48 kHz from the
   DSI3DL2LISTENER the game passes. The retail effects image's DSP program is
   not run; this is a feedback-delay approximation of the same parameters. */
enum {
    RECOMP_REVERB_RATE = 48000,
    RECOMP_REVERB_PREDELAY = 32768, /* power of two above 0.4 s plus early taps */
    RECOMP_REVERB_LINE = 4096,
    RECOMP_REVERB_ALLPASS = 1024,
};

/* DSI3DL2LISTENER: 48 bytes, guest layout. Levels in hundredths of dB,
   times in seconds, diffusion/density in percent, HF reference in Hz. */
typedef struct RecompI3dl2Listener {
    int32_t room, room_hf;
    float room_rolloff, decay_time, decay_hf_ratio;
    int32_t reflections;
    float reflections_delay;
    int32_t reverb;
    float reverb_delay, diffusion, density, hf_reference;
} RecompI3dl2Listener;

/* Zero-initialized is silent: the XDK default listener has room -10000. */
typedef struct RecompReverb {
    int enabled;
    float input_coeff, early_gain, late_gain, allpass_gain;
    uint32_t early_delay, late_delay;
    float line_gain[4], line_coeff[4];

    uint32_t quiet;
    /* Running state from here on; cleared when the tail goes idle. */
    uint32_t n;
    float input_state, line_state[4];
    float predelay[RECOMP_REVERB_PREDELAY];
    float allpass[2][RECOMP_REVERB_ALLPASS];
    float lines[4][RECOMP_REVERB_LINE];
} RecompReverb;

/* Clamps to the I3DL2 ranges; keeps the running tail. */
void recomp_reverb_set(RecompReverb *reverb, const RecompI3dl2Listener *listener);

/* Filters mono input (1.0 = full scale) into interleaved stereo wet output.
   Returns zero once the tail has drained to silence; until then submit every
   block, quiet ones included, so the host stream has no gaps. */
int recomp_reverb_render(RecompReverb *reverb, const float *input, uint32_t frames, float *out);

#endif
