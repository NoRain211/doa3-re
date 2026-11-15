#include "dsound_hrtf.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void check(int ok, const char *what)
{
    if (!ok) {
        fprintf(stderr, "FAIL dsound hrtf: %s\n", what);
        exit(1);
    }
}

int main(void)
{
    static uint8_t bank[2222 * 32], map[61 * 31 * 2];
    float azimuth, elevation;

    recomp_hrtf_angles(1.0f, 0.0f, 0.0f, &azimuth, &elevation);
    check(azimuth == 90.0f && elevation == 0.0f, "right");
    recomp_hrtf_angles(-1.0f, 0.0f, 1.0f, &azimuth, &elevation);
    check(azimuth == -45.0f, "front left diagonal");
    recomp_hrtf_angles(0.0f, 0.0f, -1.0f, &azimuth, &elevation);
    check(azimuth == 180.0f, "behind");
    recomp_hrtf_angles(0.0f, -1.0f, 0.0f, &azimuth, &elevation);
    check(azimuth == 0.0f && elevation == -90.0f, "below");

    /* Azimuth 30 at elevation 0 is map row 10, column 15; point it at pair 4. */
    map[2 * (10 * 31 + 15)] = 8;
    bank[8 * 32] = 64;       /* ear0 tap 0 */
    bank[8 * 32 + 31] = 3;   /* ear0 delay byte */
    bank[9 * 32 + 2] = 32;   /* ear1 tap 2 */
    RecompHrtfFilter right = recomp_hrtf_lookup(bank, map, 30.0f, 0.0f);
    check(right.left == (const int8_t *)bank + 8 * 32 && right.delay == 3, "right-side source delays left ear");
    RecompHrtfFilter left = recomp_hrtf_lookup(bank, map, -31.0f, 0.0f);
    check(left.left == right.right && left.right == right.left && left.delay == -3, "left side mirrors ears");

    /* A 48 kHz impulse comes out as each ear's taps, the far ear delayed. */
    RecompHrtfVoice voice;
    memset(&voice, 0, sizeof voice);
    voice.filter = voice.rendered = right;
    int16_t input[8] = {0, 27100};
    float out[16];
    float mono[8];
    check(recomp_hrtf_render(&voice, input, 8, 48000, out, mono, 8) == 8, "frame count");
    /* Output n is input n-1 (interpolation latency). Tap 64 is 1/16, 32 is 1/64. */
    check(out[2 * 2] == 0.0f && fabsf(out[2 * 5] - 27100.0f / 32768.0f / 16.0f) < 0.000001f,
        "left: tap 0 after 3-sample delay");
    check(fabsf(out[2 * 4 + 1] - 27100.0f / 32768.0f / 64.0f) < 0.000001f, "right: tap 2, undelayed");
    check(mono[1] == 0.0f && fabsf(mono[2] - 27100.0f / 32768.0f) < 0.000001f, "mono send is the unfiltered signal");

    /* 24 kHz input doubles the frame count. */
    memset(&voice, 0, sizeof voice);
    voice.filter = voice.rendered = right;
    check(recomp_hrtf_render(&voice, input, 8, 24000, out, NULL, 8) == 8, "upsampled pieces stop at capacity");

    /* Tap bytes as the APU reads them back (xemu PR #3105). */
    static const struct { uint8_t byte; float gain; } taps[] = {
        {0x7f, 31.0f / 32.0f}, {0x40, 1.0f / 16.0f}, {0x20, 1.0f / 64.0f},
        {0x01, 1.0f / 2048.0f}, {0x81, -1.0f / 2048.0f}, {0xc0, -1.0f / 16.0f}};
    for (int i = 0; i < 8; ++i) input[i] = 32767;
    for (size_t t = 0; t < sizeof taps / sizeof taps[0]; ++t) {
        const int8_t ear[32] = {(int8_t)taps[t].byte};
        memset(&voice, 0, sizeof voice);
        voice.filter = voice.rendered = (RecompHrtfFilter){(const int8_t *)ear, (const int8_t *)ear, 0};
        check(recomp_hrtf_render(&voice, input, 8, 48000, out, NULL, 8) == 8 &&
            fabsf(out[14] - taps[t].gain * 32767.0f / 32768.0f) < 0.000001f, "tap format");
    }

    /* Taps sum without normalization: two 1/16 taps pass 1/8. */
    const int8_t two[32] = {0x40, 0x40};
    memset(&voice, 0, sizeof voice);
    voice.filter = voice.rendered = (RecompHrtfFilter){two, two, 0};
    check(recomp_hrtf_render(&voice, input, 8, 48000, out, NULL, 8) == 8 &&
        fabsf(out[14] - 32767.0f / 32768.0f / 8.0f) < 0.000001f, "taps are not normalized");
    puts("PASS dsound hrtf angles, lookup, mirroring, delay, render");
    return 0;
}
