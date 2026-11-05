#include "synctest.h"

#include "cri_service_adapter.h"
#include "d3d_creation_adapter.h"
#include "d3d_draw_adapter.h"
#include "d3d_frame_adapter.h"
#include "d3d_render_state_adapter.h"
#include "d3d_texture_adapter.h"
#include "d3d_vblank.h"
#include "d3d_vertex_shader_adapter.h"
#include "dsound_api_adapter.h"
#include "dsound_service_adapter.h"
#include "fiber_adapter.h"
#include "input_adapter.h"
#include "kernel_abi.h"
#include "program_manual.h"
#include "runtime.h"
#include "save_adapter.h"
#include "save_transaction.h"
#include "soundtrack_adapter.h"
#include "xapi_time_adapter.h"
#include "xbox_memory_layout.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Guest RAM the host writes on its own schedule, not the frame's. Compares
   and desync hashes skip it; snapshots still save and restore it. */
static size_t build_ignore_ranges(RecompRamRange *ranges, size_t capacity)
{
    size_t count = 0u;
    /* Kernel data exports and synthetic thread objects. */
    if (count < capacity) ranges[count++] = (RecompRamRange){XBOX_KERNEL_DATA_BASE, XBOX_HEAP_BASE};
    const size_t fiber_stacks = count;
    count += recomp_fiber_adapter_stack_ranges(ranges + count, capacity - count);
    /* Worker threads park on their own stacks, which hold host-scheduled
       scratch. Their heap allocations and TLS blocks are compared. */
    const size_t thread_stacks = count;
    count += recomp_kernel_thread_stack_ranges(ranges + count, capacity - count);
    const size_t kernel_stacks = count;
    count += recomp_kernel_memory_stack_ranges(ranges + count, capacity - count);
    if (getenv("RECOMP_SYNCTEST_VERBOSE") != NULL) {
        for (size_t i = fiber_stacks; i < kernel_stacks; ++i)
            fprintf(stderr, "recomp synctest: %s stack +0x%zx..+0x%zx\n",
                i < thread_stacks ? "fiber" : "thread", ranges[i].start, ranges[i].stop);
    }
    if (count == capacity) {
        fprintf(stderr, "recomp synctest: ignore range list full\n");
        exit(2);
    }
    // Small list; insertion sort keeps the walk below simple.
    for (size_t i = 1u; i < count; ++i) {
        RecompRamRange value = ranges[i];
        size_t j = i;
        while (j > 0u && ranges[j - 1u].start > value.start) {
            ranges[j] = ranges[j - 1u];
            --j;
        }
        ranges[j] = value;
    }
    return count;
}

static bool ram_equal(const void *left, const void *right, size_t size)
{
    const uint8_t *a = left, *b = right;
    RecompRamRange ranges[64];
    const size_t range_count = build_ignore_ranges(ranges, 64u);
    size_t at = 0u;
    for (size_t range = 0u; range <= range_count && at < size; ++range) {
        size_t end = range < range_count && ranges[range].start < size ? ranges[range].start : size;
        if (end > at && memcmp(a + at, b + at, end - at) != 0) {
            if (getenv("RECOMP_SYNCTEST_VERBOSE") != NULL) {
                while (a[at] == b[at]) ++at;
                fprintf(stderr, "recomp synctest: ram_equal rejects +0x%zx\n", at);
            }
            return false;
        }
        if (range < range_count && ranges[range].stop > at) at = ranges[range].stop;
    }
    return true;
}

void recomp_synctest_mask_ignored(uint8_t *ram, size_t size)
{
    RecompRamRange ranges[64];
    const size_t range_count = build_ignore_ranges(ranges, 64u);
    for (size_t i = 0u; i < range_count; ++i) {
        if (ranges[i].start >= size) continue;
        const size_t stop = ranges[i].stop < size ? ranges[i].stop : size;
        memset(ram + ranges[i].start, 0, stop - ranges[i].start);
    }
}

size_t recomp_synctest_ignored_ranges(RecompRamRange *ranges, size_t capacity)
{
    return build_ignore_ranges(ranges, capacity);
}

/* Guest time plus the presenter marks derived from it; both rewind together
   or a replayed frame waits on a vblank from the abandoned timeline. */
typedef struct ClockState {
    RecompClockState clock;
    uint64_t vblank[3];
} ClockState;
static void clock_save(void *state)
{
    ClockState *out = state;
    recomp_xapi_clock_save(&out->clock);
    recomp_d3d_vblank_state_save(out->vblank);
}
static void clock_restore(const void *state)
{
    const ClockState *in = state;
    recomp_xapi_clock_restore(&in->clock);
    recomp_d3d_vblank_state_restore(in->vblank);
}
static void heap_save(void *state) { xbox_HeapStateSave(state); }
static void heap_restore(const void *state) { xbox_HeapStateRestore(state); }
static bool file_positions_restorable(const void *state, size_t size)
{
    (void)size;
    return recomp_kernel_file_state_restorable(state);
}
/* Save storage and its journal live on the host disk, outside the snapshot.
   A replay after a save sees the files the first pass wrote and takes another
   branch, and a rewind into an open transaction would replay its begin and
   never commit. Neither kind of frame is rewound. */
typedef struct SaveStorageState {
    bool pending;
    uint32_t mutations;
} SaveStorageState;
static void save_storage_save(void *state)
{
    *(SaveStorageState *)state = (SaveStorageState){
        recomp_save_pending(), recomp_kernel_profile_mutations()};
}
static void save_storage_restore(const void *state) { (void)state; }
static bool save_storage_restorable(const void *state, size_t size)
{
    const SaveStorageState *saved = state;
    (void)size;
    return !saved->pending && saved->mutations == recomp_kernel_profile_mutations();
}

/* Snapshots save and restore these rows top to bottom. A reboot resets them
   bottom to top, so a row resets before every row above it:
   - dsound, the soundtrack and cri free their guest pool blocks, so they sit
     below allocations, whose reset forgets every block;
   - guest threads delete their native fibers before the XAPI fibers do.
   A row with no reset notes what resets it; a row with no save or live only
   resets. */
static void with_runtime_modules(void (*use)(const RecompSyncTestModule *table, size_t count))
{
    const RecompSyncTestModule table[] = {
        /* Reset by the caller's recomp_runtime_init. */
        {"cpu", recomp_runtime_state_size(), recomp_runtime_state_save,
            recomp_runtime_state_restore, recomp_runtime_state_equal},
        {"clock", sizeof(ClockState), clock_save, clock_restore, .ignore = true,
            .reset = recomp_xapi_clock_reset},
        recomp_fiber_adapter_module,
        recomp_kernel_thread_module,
        /* Reset with the threads. */
        recomp_kernel_thread_fiber_module,
        {"file_positions", recomp_kernel_file_state_size(), recomp_kernel_file_state_save,
            recomp_kernel_file_state_restore, .restorable = file_positions_restorable,
            .reset = recomp_kernel_file_reset},
        {"kernel_video", .reset = recomp_kernel_video_reset},
        {"kernel_interrupt", recomp_kernel_interrupt_state_size(),
            recomp_kernel_interrupt_state_save, recomp_kernel_interrupt_state_restore,
            .reset = recomp_kernel_interrupt_reset},
        recomp_kernel_memory_module,
        {"heap", sizeof(XboxHeapState), heap_save, heap_restore, .reset = xbox_HeapReset},
        recomp_input_adapter_module,
        /* Reset with d3d_creation, which keeps the window. */
        recomp_d3d_frame_adapter_module,
        recomp_d3d_render_state_adapter_module,
        recomp_d3d_texture_adapter_module,
        recomp_d3d_draw_adapter_module,
        recomp_d3d_vertex_shader_adapter_module,
        recomp_d3d_creation_adapter_module,
        {"soundtrack", .reset = recomp_soundtrack_shutdown},
        {"dsound_api", recomp_dsound_api_state_size(), recomp_dsound_api_state_save,
            recomp_dsound_api_state_restore, .reset = recomp_dsound_api_reset},
        {"dsound_service", recomp_dsound_service_adapter_state_size(),
            recomp_dsound_service_adapter_state_save, recomp_dsound_service_adapter_state_restore,
            .reset = recomp_dsound_service_adapter_reset},
        recomp_cri_service_adapter_module,
        /* Holds nothing to reset; it reads host state. */
        {"save_storage", sizeof(SaveStorageState), save_storage_save, save_storage_restore,
            NULL, save_storage_restorable, true},
        {"save_adapter", .reset = recomp_save_adapter_reset},
        {"program_manual", .reset = recomp_program_manual_reset},
    };
    use(table, sizeof table / sizeof table[0]);
}

static void reset_modules(const RecompSyncTestModule *table, size_t count)
{
    for (size_t i = count; i-- > 0u;)
        if (table[i].reset != NULL) table[i].reset();
}

void recomp_synctest_install_default_modules(void)
{
    recomp_synctest_set_ram_equal(ram_equal);
    with_runtime_modules(recomp_synctest_set_modules);
}

/* Puts the runtime back to power-on for a reboot of the game in this process,
   with the window and audio output kept. The runner maps the XBE again. */
void recomp_reset_runtime_modules(void)
{
    with_runtime_modules(reset_modules);
}
