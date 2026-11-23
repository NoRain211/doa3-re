#include "dsound_api_adapter.h"
#include "kernel_abi.h"
#include "xapi_time_adapter.h"

#include <stdio.h>
#include <string.h>

void recomp_test_dsound_set_time(uint64_t now);
void recomp_test_dsound_set_wall(uint64_t wall);
uint32_t recomp_test_dsound_output_calls(void);
int32_t recomp_test_dsound_volume(void);
uint32_t recomp_test_dsound_slot(void);
void recomp_test_heap_reset(uint32_t cursor, int fail_after);
uint32_t recomp_test_dsound_output(uint32_t *, uint32_t *, uint32_t *, float *);

enum {
    BASE = 0x2b000000u,
    SIZE = 0x2000u,
    STACK = BASE + 0x1000u,
    DESC = BASE + 0x100u,
    FORMAT = BASE + 0x140u,
    OUT = BASE + 0x180u,
    LISTENER = BASE + 0x200u,
    DATA = BASE + 0x400u,
};

static uint8_t memory[SIZE];
/* Stands in for the game's HRTF bank and index map (0x001CEE88-0x001E130E). */
static uint8_t hrtf_table[0x12486u];

static uint32_t *at(uint32_t address)
{
    return (uint32_t *)(void *)(memory + (address - BASE));
}

static int expect(int condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "dsound api: %s\n", message);
    }
    return condition;
}

/* Calls a DOA3 wrapper replacement with stdcall arguments; returns EAX and
   reports a wrong argument pop. */
static uint32_t call(uint32_t address, const uint32_t *args, uint32_t count,
                     int *passed)
{
    recomp_runtime.registers.esp = STACK;
    for (uint32_t i = 0u; i < count; ++i) {
        *at(STACK + 4u + i * 4u) = args[i];
    }
    recomp_dsound_api_lookup_manual(address)();
    *passed &= expect(recomp_runtime.registers.esp == STACK + 4u + count * 4u,
                      "argument pop");
    return recomp_runtime.registers.eax;
}

/* A fresh 1000 Hz mono 16-bit buffer, one sample and two bytes a millisecond;
   its samples are their own indices, so a span's first sample names its place. */
static uint32_t real_clock_buffer(int *passed, uint32_t bytes)
{
    recomp_dsound_api_reset();
    recomp_test_dsound_set_time(0u);
    recomp_test_dsound_set_wall(0u);
    *at(DESC + 4u) = 0u;
    *at(DESC + 8u) = 0u; /* the storage arrives through SetBufferData */
    *at(DESC + 12u) = FORMAT;
    *at(FORMAT) = 1u | (1u << 16u);
    *at(FORMAT + 4u) = 1000u;
    *at(FORMAT + 8u) = 2000u;
    *at(FORMAT + 12u) = 2u | (16u << 16u);
    call(0x001c7ea9u, (uint32_t[]){1u, DESC, OUT, 0u}, 4u, passed);
    const uint32_t buffer = *at(OUT);
    *passed &= expect(call(0x001c7b6fu, (uint32_t[]){buffer, DATA, bytes}, 3u, passed) == 0u,
                      "real-clock buffer data");
    for (uint32_t i = 0u; i < bytes / 2u; ++i) {
        *(int16_t *)(memory + (DATA - BASE) + i * 2u) = (int16_t)i;
    }
    return buffer;
}

/* DirectSoundDoWork at a guest-clock and a real-clock instant. */
static void pump_at(uint64_t now, uint64_t wall, int *passed)
{
    recomp_test_dsound_set_time(now);
    recomp_test_dsound_set_wall(wall);
    call(0x001c760du, NULL, 0u, passed);
}

/* The last host submission's size and first sample. */
static int submitted(uint32_t bytes, float sample)
{
    uint32_t rate, channels, bits;
    float first;
    return recomp_test_dsound_output(&rate, &channels, &bits, &first) == bytes &&
        first == sample && rate == 1000u && channels == 1u && bits == 16u;
}

/* Pumps a one-shot on the guest clock and records what the game reads. */
static void cursor_sequence(uint32_t positions[4], int *passed)
{
    static const uint64_t times[4] = {20u, 60u, 120u, 200u};
    const uint32_t buffer = real_clock_buffer(passed, 200u);
    call(0x001c7b33u, (uint32_t[]){buffer, 0u, 0u, 0u}, 4u, passed);
    for (uint32_t i = 0u; i < 4u; ++i) {
        pump_at(times[i], times[i], passed);
        call(0x001c75a1u, (uint32_t[]){buffer, OUT, OUT + 4u}, 3u, passed);
        positions[i] = *at(OUT);
    }
}

/* The frame-locked clock netplay, replays and SyncTest use: the cursor the game
   reads follows guest time, while host output plays the real clock from its own
   place. */
static int real_clock_output(int *passed)
{
    uint32_t calls;

    recomp_xapi_deterministic = true;

    /* A one-shot plays on to its end while guest time stalls. */
    uint32_t buffer = real_clock_buffer(passed, 200u);
    call(0x001c7b33u, (uint32_t[]){buffer, 0u, 0u, 0u}, 4u, passed);
    calls = recomp_test_dsound_output_calls();
    pump_at(20u, 20u, passed);
    *passed &= expect(recomp_test_dsound_output_calls() == calls, "one-shot starts at the guest cursor");
    pump_at(40u, 40u, passed);
    *passed &= expect(submitted(40u, 20.0f), "one-shot plays on the real clock");
    pump_at(60u, 60u, passed);
    *passed &= expect(submitted(40u, 40.0f), "one-shot keeps playing with guest time");
    pump_at(60u, 80u, passed);
    *passed &= expect(submitted(40u, 60.0f), "one-shot plays on while guest time stalls");
    pump_at(140u, 100u, passed);
    *passed &= expect(submitted(40u, 80.0f), "one-shot plays past the guest's finished cursor");
    call(0x001c7585u, (uint32_t[]){buffer, OUT}, 2u, passed);
    *passed &= expect(*at(OUT) == 0u, "guest sees the one-shot as over");
    calls = recomp_test_dsound_output_calls();
    pump_at(140u, 120u, passed);
    *passed &= expect(recomp_test_dsound_output_calls() == calls, "one-shot stops at its own end");
    call(0x001c6b7cu, (uint32_t[]){buffer}, 1u, passed);

    /* A ring runs at most 50 ms ahead of a stalled guest cursor. */
    buffer = real_clock_buffer(passed, 2000u);
    call(0x001c7b33u, (uint32_t[]){buffer, 0u, 0u, 1u}, 4u, passed);
    pump_at(20u, 20u, passed);
    pump_at(40u, 40u, passed);
    *passed &= expect(submitted(40u, 20.0f), "ring starts at the guest cursor");
    pump_at(40u, 50u, passed);
    *passed &= expect(submitted(20u, 40.0f), "ring plays on from its own place");
    pump_at(40u, 60u, passed);
    *passed &= expect(submitted(20u, 50.0f), "ring runs ahead of the guest cursor");
    pump_at(40u, 70u, passed);
    *passed &= expect(submitted(20u, 60.0f), "ring runs ahead to 30 ms");
    pump_at(40u, 80u, passed);
    *passed &= expect(submitted(20u, 70.0f), "ring runs ahead to 40 ms");
    pump_at(40u, 90u, passed);
    *passed &= expect(submitted(20u, 80.0f), "ring reaches 50 ms ahead");
    calls = recomp_test_dsound_output_calls();
    pump_at(40u, 100u, passed);
    *passed &= expect(recomp_test_dsound_output_calls() == calls, "ring waits 50 ms ahead");

    /* A guest cursor over 100 ms behind skips output to it; the pass that
       waited at the bound had already moved output's own clock on. */
    pump_at(400u, 110u, passed);
    *passed &= expect(submitted(20u, 400.0f), "ring skips to a guest cursor 300 ms ahead");

    /* A muted span neither plays nor moves the place; a long freeze resyncs it. */
    buffer = real_clock_buffer(passed, 2000u);
    call(0x001c7b33u, (uint32_t[]){buffer, 0u, 0u, 1u}, 4u, passed);
    pump_at(20u, 20u, passed);
    pump_at(40u, 40u, passed);
    *passed &= expect(submitted(40u, 20.0f), "mute case starts at the guest cursor");
    pump_at(40u, 50u, passed);
    *passed &= expect(submitted(20u, 40.0f), "mute case plays on");
    recomp_dsound_set_output_suppressed(true);
    calls = recomp_test_dsound_output_calls();
    pump_at(40u, 60u, passed);
    pump_at(40u, 80u, passed);
    *passed &= expect(recomp_test_dsound_output_calls() == calls, "muted span plays nothing");
    recomp_dsound_set_output_suppressed(false);
    pump_at(40u, 100u, passed);
    *passed &= expect(submitted(80u, 50.0f), "output resumes where it stopped");
    recomp_dsound_set_output_suppressed(true);
    pump_at(40u, 400u, passed);
    recomp_dsound_set_output_suppressed(false);
    calls = recomp_test_dsound_output_calls();
    pump_at(40u, 410u, passed);
    *passed &= expect(recomp_test_dsound_output_calls() == calls, "a frozen ring starts again silent");
    pump_at(40u, 420u, passed);
    *passed &= expect(submitted(20u, 40.0f), "a frozen ring starts again at the guest cursor");
    call(0x001c6b7cu, (uint32_t[]){buffer}, 1u, passed);

    /* A rewind that turns a Stop into a seek reaches the same generation; the
       ring or one-shot the guest plays is heard again. */
    static uint8_t saved[262144];
    *passed &= expect(recomp_dsound_api_state_size() <= sizeof saved, "dsound state fits the test copy");
    for (uint32_t looping = 0u; looping < 2u; looping++) {
        buffer = real_clock_buffer(passed, 2000u);
        call(0x001c7b33u, (uint32_t[]){buffer, 0u, 0u, looping}, 4u, passed);
        pump_at(20u, 20u, passed);
        pump_at(40u, 40u, passed);
        recomp_dsound_api_state_save(saved);
        call(0x001c7b57u, (uint32_t[]){buffer}, 1u, passed);
        pump_at(60u, 60u, passed);
        recomp_dsound_set_output_suppressed(true); /* the re-run's frames are muted */
        recomp_dsound_api_state_restore(saved);
        call(0x001c75c1u, (uint32_t[]){buffer, 0u}, 2u, passed);
        recomp_dsound_set_output_suppressed(false);
        pump_at(60u, 70u, passed);
        pump_at(80u, 90u, passed);
        *passed &= expect(submitted(40u, 0.0f), looping ? "a ring restarted by a re-run plays again" :
            "a one-shot restarted by a re-run plays again");
        call(0x001c6b7cu, (uint32_t[]){buffer}, 1u, passed);
    }

    /* The game reads the same cursor with and without the real clock. */
    uint32_t app[4], real[4];
    recomp_xapi_deterministic = false;
    cursor_sequence(app, passed);
    recomp_xapi_deterministic = true;
    cursor_sequence(real, passed);
    *passed &= expect(memcmp(app, real, sizeof app) == 0, "guest cursor unchanged on the real clock");
    *passed &= expect(real[0] == 40u && real[1] == 120u && real[2] == 0u, "guest cursor follows guest time");
    recomp_xapi_deterministic = false;
    return *passed;
}

int recomp_dsound_api_adapter_test(void)
{
    const RecompMemoryRegion hrtf_regions[] = {
        {BASE, SIZE, memory}, {0x001cee88u, sizeof hrtf_table, hrtf_table}};
    int passed = 1;
    uint32_t buffer;

    recomp_runtime_init(hrtf_regions, 2u, NULL, 0u, NULL, 0u);
    recomp_dsound_api_reset();
    recomp_test_dsound_set_time(0u);
    passed &= expect(call(0x001c6b72u, (uint32_t[]){1u, 0u, 0u}, 3u,
        &passed) == 0u, "SetCooperativeLevel");
    passed &= expect(call(0x001c6b77u, (uint32_t[]){1u, 0u, 0u, 0u, 0u}, 5u,
        &passed) == 0u, "Unlock");
    memset(memory, 0, sizeof memory);
    *at(DESC + 12u) = FORMAT;               /* no storage: SetBufferData */
    *at(FORMAT) = 1u | (2u << 16u);
    *at(FORMAT + 4u) = 44100u;
    *at(FORMAT + 8u) = 176400u;
    *at(FORMAT + 12u) = 4u | (16u << 16u);                 /* block align */

    call(0x001c7ea9u, (uint32_t[]){1u, DESC, OUT, 0u}, 4u, &passed);
    buffer = *at(OUT);
    passed &= expect(buffer != 0u, "no buffer handle");
    passed &= expect(call(0x001c7b6fu, (uint32_t[]){buffer, DATA, 0x100u}, 3u,
                          &passed) == 0u, "SetBufferData");

    memset(memory + DATA - BASE, 1, 0x100u);

    /* Lock 0x80 bytes at 0xc0 wraps into a second span at the start. */
    call(0x001c75ddu, (uint32_t[]){buffer, 0xc0u, 0x80u, OUT, OUT + 4u,
                                   OUT + 8u, OUT + 12u, 0u}, 8u, &passed);
    passed &= expect(*at(OUT) == DATA + 0xc0u && *at(OUT + 4u) == 0x40u &&
                         *at(OUT + 8u) == DATA && *at(OUT + 12u) == 0x40u,
                     "wrapped lock");

    call(0x001c7b33u, (uint32_t[]){buffer, 0u, 0u, 1u}, 4u, &passed);
    call(0x001c7585u, (uint32_t[]){buffer, OUT}, 2u, &passed);
    passed &= expect(*at(OUT) == 5u, "looping play status");
    recomp_test_dsound_set_time(10u);
    call(0x001c760du, NULL, 0u, &passed);
    uint32_t rate, channels, bits;
    float first;
    passed &= expect(recomp_test_dsound_output(&rate, &channels, &bits, &first) == 1764u &&
        rate == 44100u && channels == 2u && bits == 16u && first == 257, "PCM pump across ring wraps");
    passed &= expect(recomp_test_dsound_volume() == -600, "2D default voice headroom");
    call(0x001c75a1u, (uint32_t[]){buffer, OUT, OUT + 4u}, 3u, &passed);
    passed &= expect(*at(OUT) == 228u && *at(OUT + 4u) == 228u, "guest/output share elapsed time");
    call(0x001c7477u, (uint32_t[]){buffer, (uint32_t)-1200}, 2u, &passed);
    recomp_test_dsound_set_time(15u);
    call(0x001c75a1u, (uint32_t[]){buffer, OUT, OUT + 4u}, 3u, &passed);
    passed &= expect(*at(OUT) == 228u && *at(OUT + 4u) == 228u, "position trails to sent audio");
    call(0x001c7afbu, (uint32_t[]){buffer, 44100u}, 2u, &passed);
    recomp_test_dsound_set_time(20u);
    call(0x001c760du, NULL, 0u, &passed);
    passed &= expect(recomp_test_dsound_volume() == -1800, "SetVolume retains voice headroom");
    passed &= expect(recomp_test_dsound_output(&rate, &channels, &bits, &first) == 1764u,
        "SetFrequency between pumps keeps unsent audio");
    call(0x001c7b57u, (uint32_t[]){buffer}, 1u, &passed);
    call(0x001c7585u, (uint32_t[]){buffer, OUT}, 2u, &passed);
    passed &= expect(*at(OUT) == 0u, "stopped status");

    call(0x001c6b7cu, (uint32_t[]){buffer}, 1u, &passed);
    recomp_test_dsound_set_time(10u);
    /* A finite loop wraps before the storage end; stopped frequency updates
       retain timing, and Xbox ADPCM positions remain compressed byte offsets. */
    *at(FORMAT) = 0x69u | (1u << 16u);
    *at(DESC + 4u) = 0x80000u; /* Mix-in voices have no default voice headroom. */
    *at(FORMAT + 4u) = 6400u;
    *at(FORMAT + 12u) = 36u | (4u << 16u);
    call(0x001c7ea9u, (uint32_t[]){1u, DESC, OUT, 0u}, 4u, &passed);
    buffer = *at(OUT);
    memset(memory + DATA - BASE, 0, 108u);
    *(int16_t *)(memory + DATA - BASE) = 1000;
    call(0x001c7b6fu, (uint32_t[]){buffer, DATA, 108u}, 3u, &passed);
    call(0x001c7565u, (uint32_t[]){buffer, 0u, 72u}, 3u, &passed);
    call(0x001c7b33u, (uint32_t[]){buffer, 0u, 0u, 1u}, 4u, &passed);
    recomp_test_dsound_set_time(40u);
    call(0x001c760du, NULL, 0u, &passed);
    passed &= expect(recomp_test_dsound_volume() == 0, "mix-in normal level unchanged");
    passed &= expect(recomp_test_dsound_output(&rate, &channels, &bits, &first) == 384u &&
        rate == 6400u && channels == 1u && bits == 16u && first == 1000, "ADPCM decoded PCM and finite loop");
    call(0x001c75a1u, (uint32_t[]){buffer, OUT, 0u}, 3u, &passed);
    passed &= expect(*at(OUT) == 36u, "ADPCM byte cursor");
    call(0x001c75c1u, (uint32_t[]){buffer, 72u}, 2u, &passed);
    recomp_test_dsound_set_time(60u);
    call(0x001c760du, NULL, 0u, &passed);
    passed &= expect(recomp_test_dsound_output(&rate, &channels, &bits, &first) == 256u &&
        first == 1000, "seek beyond finite loop wraps safely");
    call(0x001c6b7cu, (uint32_t[]){buffer}, 1u, &passed);

    /* CTRL3D mono voices: library rolloff from min distance 1, HRTF stereo at 48 kHz. */
    *at(DESC + 4u) = 0x10u;
    hrtf_table[0] = hrtf_table[32] = 64; /* Synthetic single-tap ears. */
    call(0x001c7ea9u, (uint32_t[]){1u, DESC, OUT, 0u}, 4u, &passed);
    buffer = *at(OUT);
    call(0x001c7b6fu, (uint32_t[]){buffer, DATA, 36u}, 3u, &passed);
    const float ten = 10.0f;
    uint32_t z;
    memcpy(&z, &ten, sizeof z);
    call(0x001c74d7u, (uint32_t[]){buffer, 0u, 0u, z, 0u}, 5u, &passed);
    call(0x001c7b33u, (uint32_t[]){buffer, 0u, 0u, 1u}, 4u, &passed);
    recomp_test_dsound_set_time(80u);
    call(0x001c760du, NULL, 0u, &passed);
    passed &= expect(recomp_test_dsound_volume() == -2000 &&
        recomp_test_dsound_output(&rate, &channels, &bits, &first) == 961u * 8u &&
        rate == 48000u && channels == 2u && bits == 32u && first == 0.0f,
        "3D voice rolloff and float HRTF stereo");
    recomp_test_dsound_set_time(90u);
    call(0x001c760du, NULL, 0u, &passed);
    passed &= expect(recomp_test_dsound_output(&rate, &channels, &bits, &first) == 480u * 8u &&
        first > 0.0f && first <= 1000.0f / 32768.0f, "3D pump keeps normalized float samples");

    /* DSI3DL2_ENVIRONMENT_PRESET_GENERIC: the 3D send returns on the reverb slot. */
    const int32_t generic[12] = {-1000, -100, 0, 0x3fbeb852 /* 1.49 */, 0x3f547ae1 /* 0.83 */,
        -2602, 0x3be56042 /* 0.007 */, 200, 0x3c343958 /* 0.011 */,
        0x42c80000 /* 100 */, 0x42c80000, 0x459c4000 /* 5000 */};
    memcpy(at(LISTENER), generic, sizeof generic);
    passed &= expect(call(0x001c7457u, (uint32_t[]){1u, LISTENER, 1u}, 3u, &passed) == 0u, "SetI3DL2Listener");
    recomp_test_dsound_set_time(100u);
    call(0x001c760du, NULL, 0u, &passed);
    recomp_test_dsound_set_time(140u);
    call(0x001c760du, NULL, 0u, &passed);
    passed &= expect(recomp_test_dsound_slot() == 256u &&
        recomp_test_dsound_output(&rate, &channels, &bits, &first) >= 480u * 8u &&
        rate == 48000u && channels == 2u && bits == 32u && recomp_test_dsound_volume() == 0,
        "3D voice feeds the I3DL2 reverb return");
    *at(LISTENER) = (uint32_t)-10000;
    call(0x001c7457u, (uint32_t[]){1u, LISTENER, 1u}, 3u, &passed);
    call(0x001c6b7cu, (uint32_t[]){buffer}, 1u, &passed);
    /* Repeated owned buffers must reuse released storage, even when no new
       backing allocation is possible. SetBufferData does not transfer ownership. */
    static uint8_t heap[0x200000u];
    const RecompMemoryRegion regions[] = {
        {BASE, SIZE, memory}, {0x27000000u, sizeof heap, heap},
    };
    recomp_runtime_init(regions, 2u, NULL, 0u, NULL, 0u);
    recomp_test_heap_reset(0x27001000u, -1);
    *at(DESC + 8u) = 0x10000u;
    *at(FORMAT) = 1u | (2u << 16u);
    *at(FORMAT + 12u) = 4u | (16u << 16u);
    for (unsigned i = 0; i < 3; ++i) {
        passed &= expect(call(0x001c7ea9u, (uint32_t[]){1u, DESC, OUT, 0u},
            4u, &passed) == 0u, "owned buffer creation");
        buffer = *at(OUT);
        call(0x001c75ddu, (uint32_t[]){buffer, 0u, 4u, OUT, OUT + 4u,
            OUT + 8u, OUT + 12u, 0u}, 8u, &passed);
        passed &= expect((*at(OUT) & 31u) == 0u &&
            *recomp_memory_u32(*at(OUT)) == 0u, "owned storage aligned and cleared");
        *recomp_memory_u32(*at(OUT)) = 0x12345678u;
        call(0x001c7b6fu, (uint32_t[]){buffer, DATA, 0x100u}, 3u, &passed);
        call(0x001c6b7cu, (uint32_t[]){buffer}, 1u, &passed);
        recomp_test_heap_reset(0x27011020u, 0);
    }
    recomp_test_dsound_set_time(0u);
    passed &= real_clock_output(&passed);
    recomp_test_dsound_set_time(0u);
    recomp_runtime_init(NULL, 0u, NULL, 0u, NULL, 0u);
    return passed;
}
