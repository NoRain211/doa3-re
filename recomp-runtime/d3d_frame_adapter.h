#ifndef DOAXBV_RECOMP_D3D_FRAME_ADAPTER_H
#define DOAXBV_RECOMP_D3D_FRAME_ADAPTER_H

#include "d3d_presenter.h"
#include "runtime.h"
#include "synctest.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
/* Safe from the console control thread; cleanup occurs at a frame boundary. */
void recomp_d3d_frame_adapter_request_console_break(void);
#endif
void recomp_d3d_frame_adapter_initialize(
    const RecompD3dPresenterConfig *config,
    uint32_t device_address);
/* Puts the frame model back to power-on for a reboot of the game. The
   presenter and its window stay for the next boot's device. */
void recomp_d3d_frame_adapter_reset(void);
/* Destroys the presenter and its window, then resets. */
void recomp_d3d_frame_adapter_close_window(void);
/* Applies the normal host-window exit to any presenter operation. */
void recomp_d3d_frame_adapter_exit_if_closed(RecompD3dPresenterError error);
void recomp_d3d_frame_adapter_reset_buffers(void);
/* The presenter created by this adapter, or NULL before initialization.
   The draw seam submits through the same presenter so clear, draw, and
   present stay ordered on one owning thread. */
RecompD3dPresenter *recomp_d3d_frame_adapter_presenter(void);
/* Submits to that presenter, dropping what the suppression flags hide. */
RecompD3dPresenterError recomp_d3d_frame_adapter_submit(const RecompD3dPresenterCommand *command);
/* A frame run again for rollback, or one played out of sight, still runs the
   guest's Present wait (it moves guest time) but shows nothing: present drops
   the flip, gpu_work drops clears and draws. Gamma always applies. */
void recomp_d3d_frame_adapter_set_suppressed(bool present, bool gpu_work);
/* Shows the last presented frame again with draw's shapes over it, for a host
   panel while the guest waits at a frame boundary. Not suppressible. */
void recomp_d3d_frame_adapter_present_again(void (*draw)(const RecompD3dPresenterTarget *target));
/* The guest return address of the latest Present or Swap. A frame snapshot
   rewinds only from the same call site, where the host stack matches. */
uint32_t recomp_d3d_frame_adapter_swap_caller(void);
/* The swap model, whose counter reaches guest RAM, for a frame snapshot. */
extern const RecompSyncTestModule recomp_d3d_frame_adapter_module;
bool recomp_d3d_frame_adapter_target(RecompD3dPresenterTarget *target);
/* The back buffer's size, which its target leaves out. */
void recomp_d3d_frame_adapter_size(uint32_t *width, uint32_t *height);
/* The guest's swap counter, so seams that run per draw can tell which frame
   they are in. Diagnostics only; nothing in the frame path depends on it. */
uint32_t recomp_d3d_frame_adapter_swap_counter(void);
void recomp_d3d_clear_adapter(void);
void recomp_d3d_present_adapter(void);
RecompFunction recomp_d3d_frame_lookup_manual(uint32_t guest_address);
/* Optional host hook run once per Present, after the frame counter updates. */
extern void (*recomp_d3d_present_hook)(uint32_t swap_counter);
/* Optional host hook run as Present or Swap returns to the guest, with its
   result registers set: the frame boundary SyncTest and rollback use. */
extern void (*recomp_d3d_frame_boundary_hook)(uint32_t swap_counter);
/* Optional host hooks that draw over a shown frame's target just before its
   Present, in this order. Each owner sets only its own slot. */
enum {
    RECOMP_D3D_OVERLAY_PERF,    /* the runner's frame rate readout */
    RECOMP_D3D_OVERLAY_NETPLAY, /* the netplay menu or session screens */
    RECOMP_D3D_OVERLAY_HOOKS,
};
extern void (*recomp_d3d_overlay_hooks[RECOMP_D3D_OVERLAY_HOOKS])(const RecompD3dPresenterTarget *target);

#ifdef __cplusplus
}
#endif

#endif
