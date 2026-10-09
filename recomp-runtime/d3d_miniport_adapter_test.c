#include "d3d_miniport_adapter.h"

#include <stdio.h>
#include <string.h>

enum {
    BASE = 0x2a000000u,
    SIZE = 0x4000u,
    DEVICE = BASE,
    CHANNEL = BASE + 0x3000u,
    CONTEXT = BASE + 0x3100u,
};

static uint8_t memory[SIZE];

static uint32_t *at(uint32_t address)
{
    return (uint32_t *)(void *)(memory + (address - BASE));
}

static int expect(int condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "d3d miniport: %s\n", message);
    }
    return condition;
}

int recomp_d3d_miniport_adapter_test(void)
{
    const RecompMemoryRegion region = {BASE, SIZE, memory};
    int passed = 1;

    recomp_runtime_init(&region, 1u, NULL, 0u, NULL, 0u);
    memset(memory, 0, sizeof memory);
    *at(DEVICE + 0x0000u) = 0x83dad120u;   /* put */
    *at(DEVICE + 0x0400u) = 0x83dbb000u;   /* alternate put */
    *at(DEVICE + 0x001cu) = 9u;            /* next fence */
    *at(DEVICE + 0x03f0u) = CONTEXT;
    *at(DEVICE + 0x2304u) = CHANNEL;
    *at(DEVICE + 0x2b60u) = 7u;            /* frame counter */

    recomp_d3d_miniport_kick_off(DEVICE);
    passed &= expect(*at(CHANNEL + 0x40u) == 0x03dad120u &&
                         *at(CHANNEL + 0x44u) == 0x03dad120u,
                     "DMA put/get not both at the kicked put");
    passed &= expect(*at(DEVICE + 0x18u) == 0x83dad120u, "kicked put");
    passed &= expect(*at(CONTEXT) == 7u, "fences not completed");
    passed &= expect(*at(DEVICE + 0x2518u) == 7u, "flips not completed");

    *at(DEVICE + 0x000cu) = 4u;            /* alternate pusher */
    recomp_d3d_miniport_kick_off(DEVICE);
    passed &= expect(*at(CHANNEL + 0x40u) == 0x03dbb000u &&
                         *at(DEVICE + 0x18u) == 0x83dbb000u,
                     "alternate pusher ignored");

    /* InsertFence records the fence, kicks, then increments the counter. */
    *at(DEVICE + 0xe4u + ((9u >> 1u) & 63u) * 12u) = 9u;
    recomp_d3d_miniport_kick_off(DEVICE);
    passed &= expect(*at(CONTEXT) == 9u,
                     "fence issued before counter increment not completed");
    *at(DEVICE + 0x001cu) = 11u;
    recomp_d3d_miniport_kick_off(DEVICE);
    passed &= expect(*at(CONTEXT) == 9u,
                     "unissued next fence completed prematurely");

    passed &= expect(recomp_d3d_miniport_lookup_manual(0x001b88c0u) != NULL &&
                         recomp_d3d_miniport_lookup_manual(0x001b88c1u) == NULL,
                     "kick-off lookup");
    recomp_runtime_init(NULL, 0u, NULL, 0u, NULL, 0u);
    return passed;
}
