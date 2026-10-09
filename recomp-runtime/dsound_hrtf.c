#include "dsound_hrtf.h"

#include <math.h>
#include <stdlib.h>

void recomp_hrtf_angles(float x, float y, float z, float *azimuth, float *elevation)
{
    /* 0x001C9DA1: piecewise ratio approximations, not atan2/asin. */
    float h = sqrtf(x * x + z * z), ax = fabsf(x), az = fabsf(z), ay = fabsf(y);
    float a = 0.0f, e;

    if (h != 0.0f) {
        a = az > ax ? 45.0f * ax / az : 90.0f - 45.0f * az / ax;
        if (z < 0.0f) a = 180.0f - a;
        if (x < 0.0f) a = -a;
    }
    if (h == 0.0f && ay == 0.0f) e = 0.0f;
    else e = ay < h ? 45.0f * ay / h : 90.0f - 45.0f * h / ay;
    *azimuth = a;
    *elevation = y < 0.0f ? -e : e;
}

/* 0x001C96C0: x87 FISTP of t - 0.5 (round to nearest even), then +1 when negative. */
static int quantize(float t)
{
    int value = (int)nearbyintf(t - 0.5f);
    return value < 0 ? value + 1 : value;
}

RecompHrtfFilter recomp_hrtf_lookup(const uint8_t *bank, const uint8_t *map,
    float azimuth, float elevation)
{
    /* 0x001CB509 with fold_rear = 0. */
    int e = 6 * (quantize(elevation + (elevation >= 0.0f ? 3.0f : -3.0f)) / 6);
    int a = 0;
    if (e < -90 || e > 90) e = 0;
    if (abs(e) != 90) {
        int step = abs(e) > 60 ? 12 : abs(e) > 30 ? 6 : 3;
        a = step * (quantize(fabsf(azimuth) + step / 2.0f) / step);
        if (a < 0 || a > 180) a = 0;
    }
    const uint8_t *entry = map + 2 * ((a / 3) * 31 + (e + 90) / 6);
    const int8_t *ear0 = (const int8_t *)bank + 32 * (entry[0] | entry[1] << 8);
    /* 0x001CA4D4: the canonical record's delay byte, negated with the ears. */
    if (azimuth >= 0.0f) return (RecompHrtfFilter){ear0, ear0 + 32, ear0[31]};
    return (RecompHrtfFilter){ear0 + 32, ear0, -ear0[31]};
}

/* DSOUND uploads the table bytes unchanged (0x001CA481); the APU's scale is
   unknown. ponytail: xemu's per-ear L1 normalization (hw/xbox/mcpx/apu/vp/
   hrtf.h, 478b4f4) caps each ear at its input peak but shrinks interaural
   level differences; swap in a hardware-measured scale if one appears. */
static float tap(const RecompHrtfVoice *voice, const int8_t *ear, int delay)
{
    float sum = 0.0f, l1 = 0.0f;
    if (ear == NULL) return 0.0f;
    for (int k = 0; k < RECOMP_HRTF_TAPS; ++k) {
        sum += ear[k] * voice->history[(voice->head - (uint32_t)(k + delay)) % RECOMP_HRTF_HISTORY];
        l1 += fabsf((float)ear[k]);
    }
    return l1 > 0.0f ? sum / l1 : 0.0f;
}

uint32_t recomp_hrtf_render(RecompHrtfVoice *voice, const int16_t *input,
    uint32_t input_frames, uint32_t source_rate, float *out, uint32_t out_frames)
{
    const double step = (double)source_rate / RECOMP_HRTF_RATE;
    const RecompHrtfFilter from = voice->rendered, to = voice->filter;
    const int fade = from.left != to.left || from.right != to.right || from.delay != to.delay;
    const uint32_t total = (uint32_t)((input_frames - voice->phase) / step) + 1u;
    uint32_t written = 0u, consumed = 0u;

    /* Linear interpolation between the previous and next source sample. */
    while (consumed < input_frames && written < out_frames) {
        if (voice->phase >= 1.0) {
            voice->previous = input[consumed++];
            voice->phase -= 1.0;
            continue;
        }
        float next = input[consumed];
        voice->head = (voice->head + 1u) % RECOMP_HRTF_HISTORY;
        voice->history[voice->head] = voice->previous + (float)voice->phase * (next - voice->previous);
        voice->phase += step;

        float left = tap(voice, to.left, to.delay > 0 ? to.delay : 0);
        float right = tap(voice, to.right, to.delay < 0 ? -to.delay : 0);
        if (fade) {
            float mix = (float)(written + 1u) / (float)(total > written ? total : written + 1u);
            left = left * mix + tap(voice, from.left, from.delay > 0 ? from.delay : 0) * (1.0f - mix);
            right = right * mix + tap(voice, from.right, from.delay < 0 ? -from.delay : 0) * (1.0f - mix);
        }
        out[2u * written] = left / 32768.0f;
        out[2u * written + 1u] = right / 32768.0f;
        ++written;
    }
    while (consumed < input_frames) voice->previous = input[consumed++], voice->phase -= 1.0;
    voice->rendered = to;
    return written;
}
