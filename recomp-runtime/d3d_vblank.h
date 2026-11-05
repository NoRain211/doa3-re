#ifndef DOAXBV_RECOMP_D3D_VBLANK_H
#define DOAXBV_RECOMP_D3D_VBLANK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void recomp_d3d_vblank_reset(void);
// The presenter's three vblank marks are guest time, so a state rewind restores them.
void recomp_d3d_vblank_state_save(uint64_t *state);
void recomp_d3d_vblank_state_restore(const uint64_t *state);
void recomp_d3d_wait_vblank(void);
void recomp_d3d_wait_present(void);
// Sleeps the calling thread until a steady_clock time, in nanoseconds since its epoch.
void recomp_d3d_sleep_until(long long steady_ns);

#ifdef __cplusplus
}
#endif

#endif
