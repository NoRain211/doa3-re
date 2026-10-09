#include "dsound_api_adapter.h"
#include "kernel_abi.h"

#include <stdio.h>
#include <string.h>

void recomp_test_dsound_set_time(uint64_t now);
int32_t recomp_test_dsound_volume(void);
void recomp_test_heap_reset(uint32_t cursor, int fail_after);
uint32_t recomp_test_dsound_output(uint32_t *, uint32_t *, uint32_t *, float *);

enum {
    BASE = 0x2b000000u,
    SIZE = 0x2000u,
    STACK = BASE + 0x1000u,
    DESC = BASE + 0x100u,
    FORMAT = BASE + 0x140u,
    OUT = BASE + 0x180u,
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
    recomp_runtime_init(NULL, 0u, NULL, 0u, NULL, 0u);
    return passed;
}
