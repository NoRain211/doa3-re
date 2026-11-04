#ifndef DOAXBV_XBOX_MEMORY_LAYOUT_H
#define DOAXBV_XBOX_MEMORY_LAYOUT_H

#include <stdbool.h>
#include <stdint.h>

/* Kernel data exports and synthetic thread objects. Above the main stack top
   (0x00f7fff0) and below the heap; the runner rejects an XBE image that
   reaches it. DOAXBV used 0x00740000, which is inside DOA3's .data BSS. */
#define XBOX_KERNEL_DATA_BASE 0x00f80000u
#define XBOX_STARTUP_THREAD_OBJECT (XBOX_KERNEL_DATA_BASE + 0x500u)
#define XBOX_STARTUP_THREAD_STACK_SLOT 0x03ffffc0u

#ifdef __cplusplus
extern "C" {
#endif

bool xbox_HeapSetImageEnd(uint32_t image_end);
uint32_t xbox_HeapAlloc(uint32_t size, uint32_t alignment);
void xbox_HeapFree(uint32_t guest_address);
uint32_t xbox_HeapCheckpoint(void);
bool xbox_HeapRestore(uint32_t checkpoint);
/* The allocator cursors, for a frame snapshot. */
typedef struct XboxHeapState {
    uint32_t cursor;
    uint32_t contiguous_cursor;
    uint32_t image_gap_floor;
    uint32_t image_gap_cursor;
} XboxHeapState;
void xbox_HeapStateSave(XboxHeapState *state);
void xbox_HeapStateRestore(const XboxHeapState *state);
/* Forgets the image end and every allocation, for a reboot. */
void xbox_HeapReset(void);
uint32_t xbox_ContiguousAlloc(
    uint32_t size,
    uint32_t lowest_address,
    uint32_t highest_address,
    uint32_t alignment);

#ifdef __cplusplus
}
#endif

#endif
