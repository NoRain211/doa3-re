#include "dsound_reverb.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

static const uint32_t line_length[4] = {1693u, 2137u, 2683u, 3203u}; /* 35-67 ms */
static const uint32_t allpass_length[2] = {225u, 556u};
/* The effects image's reflections, measured in xemu (tools/dsp_bench): a
   tap pair per ear repeating at x0.4. */
static const uint32_t early_tap[2][6] = {{0u, 206u, 336u, 542u, 672u, 878u}, {0u, 137u, 305u, 442u, 610u, 747u}};
static const float early_weight[2][6] = {{1.0f, 1.0f, -0.4f, -0.4f, 0.16f, 0.16f}, {1.0f, 1.0f, 0.4f, 0.4f, 0.16f, 0.16f}};
/* Its late onset, measured the same way on USA Room and Hallway: at
   ReflectionsDelay + ReverbDelay a +/- tap pair per ear, repeated once 60/70 ms
   later at 0.8x the decay over that time; the diffuse tail follows. Each entry
   is {+ tap, - tap} in samples after the onset. */
static const uint32_t pair_tap[2][2] = {{0u, 120u}, {466u, 274u}};
static const uint32_t repeat_tap[2][2] = {{3058u, 2904u}, {3830u, 3370u}};
enum { TAIL_DELAY = 2400u }; /* the network's input, 50 ms after the onset */

static float clampf(float v, float lo, float hi) { return fminf(fmaxf(v, lo), hi); }
static float millibels(float mb) { return powf(10.0f, mb / 2000.0f); }

/* One-pole lowpass y += (1 - a)(x - y) with unit DC gain and energy gain g2 at
   cos(w) = c: the smaller root of (1 - a)^2 = g2 (1 - 2ac + a^2). */
static float lowpass_coeff(float g2, float c)
{
    if (g2 >= 0.9999f) return 0.0f;
    g2 = fmaxf(g2, 0.000001f);
    return (1.0f - g2 * c - sqrtf(2.0f * g2 * (1.0f - c) - g2 * g2 * (1.0f - c * c))) / (1.0f - g2);
}

void recomp_reverb_set(RecompReverb *r, const RecompI3dl2Listener *p)
{
    /* shortcut: density and room rolloff are ignored (DOA3's presets use
       density 100 and rolloff 0); model them when a caller varies them. */
    const float room = clampf((float)p->room, -10000.0f, 0.0f);
    const float room_hf = clampf((float)p->room_hf, -10000.0f, 0.0f);
    /* Lines fall 60 dB per DecayTime, as DSOUND sets them; xemu's DSP JIT
       measures that on USA Room and Hallway (tools/dsp_bench).
       shortcut: Japan's DSOUND rings its 0.2 s preset for about 1 s on the
       bench, 25 dB under its reflections; model it if it is heard. */
    const float decay = clampf(p->decay_time, 0.1f, 20.0f);
    /* shortcut: HF ratios above 1 decay like 1 (a one-pole cannot boost HF);
       model it if a preset's bright tail is audibly short. */
    const float hf_ratio = clampf(p->decay_hf_ratio, 0.1f, 2.0f);
    const float reflections = clampf((float)p->reflections, -10000.0f, 1000.0f);
    const float reflections_delay = clampf(p->reflections_delay, 0.0f, 0.3f);
    const float reverb = clampf((float)p->reverb, -10000.0f, 2000.0f);
    const float reverb_delay = clampf(p->reverb_delay, 0.0f, 0.1f);
    const float diffusion = clampf(p->diffusion, 0.0f, 100.0f);
    const float hf_reference = clampf(p->hf_reference, 20.0f, 20000.0f);
    const float c = cosf(6.2831853f * hf_reference / RECOMP_REVERB_RATE);

    r->enabled = room > -10000.0f;
    if (!r->enabled) memset(&r->n, 0, sizeof *r - offsetof(RecompReverb, n)); /* no stale room on re-enable */
    r->input_coeff = lowpass_coeff(millibels(room_hf) * millibels(room_hf), c);
    r->early_delay = (uint32_t)lrintf(reflections_delay * RECOMP_REVERB_RATE);
    r->late_delay = r->early_delay + (uint32_t)lrintf(reverb_delay * RECOMP_REVERB_RATE);
    r->allpass_gain = 0.7f * diffusion / 100.0f;
    r->early_gain = millibels(room + reflections) * 0.38f;
    float mean = 0.0f;
    for (int i = 0; i < 4; ++i) {
        const float g = powf(10.0f, -3.0f * line_length[i] / (decay * RECOMP_REVERB_RATE));
        const float g_hf = powf(10.0f, -3.0f * line_length[i] / (decay * hf_ratio * RECOMP_REVERB_RATE));
        r->line_gain[i] = g;
        r->line_coeff[i] = lowpass_coeff((g_hf / g) * (g_hf / g), c);
        mean += g / 4.0f;
    }
    /* The network holds 1 / (1 - g^2) of the input energy and each ear reads
       half of it; scale so the late energy equals the reverb level.
       shortcut: the effect's tail falls further below that as DecayTime
       shortens; 0.85 (DecayTime / 1.49)^1.43 matches Room and Hallway on the
       bench within 2 dB per window, and is held at Hallway's value above
       1.49 s rather than extrapolated. Refit when another preset is measured. */
    r->late_gain = 0.85f * powf(fminf(decay, 1.49f) / 1.49f, 1.43f) * 1.2f * millibels(room + reverb) *
        sqrtf(2.0f * (1.0f - mean * mean));
    /* Fitted to the pair's energy on the bench: the effect holds each sample
       twice at 24 kHz, so one 48 kHz tap needs more than unity gain. */
    r->pair_gain = 1.2f * millibels(room + reverb);
    for (int c = 0; c < 2; ++c)
        r->repeat_gain[c] = 0.8f * r->pair_gain * powf(10.0f, -3.0f * repeat_tap[c][1] / (decay * RECOMP_REVERB_RATE));
}

int recomp_reverb_render(RecompReverb *r, const float *input, uint32_t frames, float *out)
{
    const uint32_t pm = RECOMP_REVERB_PREDELAY - 1u, lm = RECOMP_REVERB_LINE - 1u, am = RECOMP_REVERB_ALLPASS - 1u;
    const uint32_t drained = r->late_delay + TAIL_DELAY + RECOMP_REVERB_LINE;
    int any_input = 0;
    for (uint32_t i = 0u; i < frames && !any_input; ++i) any_input = input[i] != 0.0f;
    if (!r->enabled || (!any_input && r->quiet > drained)) {
        memset(out, 0, (size_t)frames * 2u * sizeof *out);
        return 0;
    }
    float peak = 0.0f;
    for (uint32_t i = 0u; i < frames; ++i) {
        const uint32_t n = r->n++;
        r->input_state += (1.0f - r->input_coeff) * (input[i] - r->input_state);
        r->predelay[n & pm] = r->input_state;

        const uint32_t e = n - r->early_delay;
        float ear[2] = {0.0f, 0.0f};
        for (int c = 0; c < 2; ++c)
            for (int k = 0; k < 6; ++k) ear[c] += early_weight[c][k] * r->predelay[(e - early_tap[c][k]) & pm];
        float left = r->early_gain * ear[0];
        float right = r->early_gain * ear[1];

        const uint32_t onset = n - r->late_delay;
        const float *pd = r->predelay;
        left += r->pair_gain * (pd[(onset - pair_tap[0][0]) & pm] - pd[(onset - pair_tap[0][1]) & pm]) +
            r->repeat_gain[0] * (pd[(onset - repeat_tap[0][0]) & pm] - pd[(onset - repeat_tap[0][1]) & pm]);
        right += r->pair_gain * (pd[(onset - pair_tap[1][0]) & pm] - pd[(onset - pair_tap[1][1]) & pm]) +
            r->repeat_gain[1] * (pd[(onset - repeat_tap[1][0]) & pm] - pd[(onset - repeat_tap[1][1]) & pm]);

        float x = pd[(onset - TAIL_DELAY) & pm];
        for (int k = 0; k < 2; ++k) {
            const float delayed = r->allpass[k][(n - allpass_length[k]) & am];
            const float v = x - r->allpass_gain * delayed;
            r->allpass[k][n & am] = v;
            x = delayed + r->allpass_gain * v;
        }

        float d[4], s[4];
        for (int k = 0; k < 4; ++k) {
            d[k] = r->lines[k][(n - line_length[k]) & lm];
            r->line_state[k] += (1.0f - r->line_coeff[k]) * (d[k] - r->line_state[k]);
            s[k] = r->line_state[k] * r->line_gain[k];
        }
        /* Orthogonal Hadamard feedback; the input enters every line. */
        const float in = 0.5f * x;
        r->lines[0][n & lm] = 0.5f * (s[0] + s[1] + s[2] + s[3]) + in;
        r->lines[1][n & lm] = 0.5f * (s[0] - s[1] + s[2] - s[3]) + in;
        r->lines[2][n & lm] = 0.5f * (s[0] + s[1] - s[2] - s[3]) + in;
        r->lines[3][n & lm] = 0.5f * (s[0] - s[1] - s[2] + s[3]) + in;
        left += r->late_gain * (d[0] + d[2]);
        right += r->late_gain * (d[1] + d[3]);

        out[2u * i] = left;
        out[2u * i + 1u] = right;
        peak = fmaxf(peak, fmaxf(fabsf(left), fabsf(right)));
    }
    /* Idle once the predelay has drained and the tail is below -100 dB;
       clearing then also keeps the feedback out of denormal range. */
    r->quiet = any_input || peak >= 0.00001f ? 0u : r->quiet + frames;
    if (r->quiet > drained) {
        memset(&r->n, 0, sizeof *r - offsetof(RecompReverb, n));
        return 0;
    }
    return 1;
}
