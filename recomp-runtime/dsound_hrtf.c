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
    if (azimuth >= 0.0f) return recomp_hrtf_filter(ear0, ear0 + 32, ear0[31]);
    return recomp_hrtf_filter(ear0 + 32, ear0, -ear0[31]);
}

/* The table's taps are sign-magnitude bytes: no tap is 0xF0-0xFF (-16..-1 in
   two's complement) while 13% are 0x80-0x8F, and read so every filter is a
   smooth decaying response. Two's complement turned small negatives into
   full-scale spikes. */
static float coeff(int8_t b)
{
    const int v = (uint8_t)b;
    return (float)(v & 0x80 ? -(v & 0x7f) : v);
}

/* DSOUND uploads the table bytes unchanged (0x001CA481); the APU's scale is
   unknown. ponytail: xemu's per-ear L1 normalization (hw/xbox/mcpx/apu/vp/
   hrtf.h, 478b4f4) caps each ear at its input peak but shrinks interaural
   level differences; swap in a hardware-measured scale if one appears.
   Returns the ear's energy gain for white noise. */
static float decode_ear(const int8_t *ear, float *taps)
{
    float l1 = 0.0f, l2 = 0.0f;
    for (int k = 0; k < RECOMP_HRTF_TAPS; ++k) {
        taps[k] = ear ? coeff(ear[k]) : 0.0f;
        l1 += fabsf(taps[k]);
    }
    for (int k = 0; l1 > 0.0f && k < RECOMP_HRTF_TAPS; ++k) {
        taps[k] /= l1;
        l2 += taps[k] * taps[k];
    }
    return l2;
}

RecompHrtfFilter recomp_hrtf_filter(const int8_t *left, const int8_t *right, int delay)
{
    RecompHrtfFilter filter = {left, right, delay};
    /* The I3DL2 send takes the pair's gain so the reverb keeps the level an
       unfiltered voice would give it: the normalization leaves a frontal
       voice about 12 dB down, and an unscaled send was heard as echo on
       yells (#108). shortcut: fitted by ear, not measured; drop it with the
       normalization when the hardware scale is known. */
    filter.gain = sqrtf((decode_ear(left, filter.taps[0]) + decode_ear(right, filter.taps[1])) / 2.0f);
    return filter;
}

static float tap(const RecompHrtfVoice *voice, const float *taps, int delay)
{
    float sum = 0.0f;
    for (int k = 0; k < RECOMP_HRTF_TAPS; ++k)
        sum += taps[k] * voice->history[(voice->head - (uint32_t)(k + delay)) % RECOMP_HRTF_HISTORY];
    return sum;
}

uint32_t recomp_hrtf_render(RecompHrtfVoice *voice, const int16_t *input,
    uint32_t input_frames, uint32_t source_rate, float *out, float *send, uint32_t out_frames)
{
    const double step = (double)source_rate / RECOMP_HRTF_RATE;
    const RecompHrtfFilter *from = &voice->rendered, *to = &voice->filter;
    const int fade = from->left != to->left || from->right != to->right || from->delay != to->delay;
    const double span = (input_frames - voice->phase) / step;
    const uint32_t total = (uint32_t)(span > 0.0 ? span : 0.0) + 1u;
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

        float left = tap(voice, to->taps[0], to->delay > 0 ? to->delay : 0);
        float right = tap(voice, to->taps[1], to->delay < 0 ? -to->delay : 0);
        if (fade) {
            float mix = (float)(written + 1u) / (float)(total > written ? total : written + 1u);
            left = left * mix + tap(voice, from->taps[0], from->delay > 0 ? from->delay : 0) * (1.0f - mix);
            right = right * mix + tap(voice, from->taps[1], from->delay < 0 ? -from->delay : 0) * (1.0f - mix);
        }
        out[2u * written] = left / 32768.0f;
        out[2u * written + 1u] = right / 32768.0f;
        if (send) send[written] = voice->history[voice->head] * to->gain / 32768.0f;
        ++written;
    }
    while (consumed < input_frames) voice->previous = input[consumed++], voice->phase -= 1.0;
    voice->rendered = voice->filter;
    return written;
}
