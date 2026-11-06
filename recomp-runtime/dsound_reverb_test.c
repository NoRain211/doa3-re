#include "dsound_reverb.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static void check(int ok, const char *what)
{
    if (!ok) {
        fprintf(stderr, "FAIL dsound reverb: %s\n", what);
        exit(1);
    }
}

static double energy(const float *stereo, uint32_t from, uint32_t to)
{
    double sum = 0.0;
    for (uint32_t i = from; i < to; ++i) sum += stereo[2u * i] * stereo[2u * i];
    return sum;
}

enum { FRAMES = RECOMP_REVERB_RATE * 2 };
static RecompReverb reverb;
static float input[FRAMES], out[2u * FRAMES];

int main(void)
{
    input[0] = 1.0f;
    check(!recomp_reverb_render(&reverb, input, 64u, out) && out[0] == 0.0f, "default listener is silent");

    /* DSI3DL2_ENVIRONMENT_PRESET_GENERIC with a flat decay. */
    RecompI3dl2Listener generic = {-1000, 0, 0.0f, 1.49f, 1.0f, -2602, 0.007f, 200, 0.011f, 100.0f, 100.0f, 5000.0f};
    recomp_reverb_set(&reverb, &generic);
    check(recomp_reverb_render(&reverb, input, FRAMES, out), "impulse is audible");
    check(out[2u * 335u] == 0.0f && out[2u * 336u] != 0.0f, "reflections delay 7 ms");

    /* The hardware tail falls 60 dB per 3x decay time: 10 dB across half of it. */
    const uint32_t a = 4800u, b = a + (uint32_t)(1.49f * RECOMP_REVERB_RATE / 2.0f), w = 2400u;
    double drop = 10.0 * log10(energy(out, a, a + w) / energy(out, b, b + w));
    check(drop > 7.0 && drop < 13.0, "decay time");

    /* Room -1000 + reverb 200: late energy about -6 dB relative to the impulse. */
    double level = 10.0 * log10(energy(out, 1000u, FRAMES));
    check(level > -12.0 && level < -4.0, "late level");

    /* The tail drains to idle and stays bit-silent. */
    input[0] = 0.0f;
    for (int i = 0; i < 40; ++i) recomp_reverb_render(&reverb, input, FRAMES, out);
    check(!recomp_reverb_render(&reverb, input, 64u, out) && out[0] == 0.0f && reverb.lines[0][1] == 0.0f, "idle");

    /* Out-of-range guest values clamp instead of running away. */
    RecompI3dl2Listener wild = {5000, 5000, 0.0f, NAN, 99.0f, 5000, 9.0f, 9000, -1.0f, 900.0f, 0.0f, 0.0f};
    recomp_reverb_set(&reverb, &wild);
    input[0] = 1.0f;
    recomp_reverb_render(&reverb, input, FRAMES, out);
    for (uint32_t i = 0u; i < 2u * FRAMES; ++i) check(isfinite(out[i]) && fabsf(out[i]) < 100.0f, "clamped parameters stay bounded");
    printf("PASS dsound reverb silence, delay, decay %.1f dB, level %.1f dB, idle, clamping\n", drop, level);
    return 0;
}
