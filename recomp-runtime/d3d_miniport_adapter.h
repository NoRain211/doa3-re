#ifndef DOA3_RECOMP_D3D_MINIPORT_ADAPTER_H
#define DOA3_RECOMP_D3D_MINIPORT_ADAPTER_H

#include "runtime.h"

/* DOA3 (D3D8 3925): the GPU-facing layer under CDevice::Init and the
   push-buffer kick-off. Everything above it stays generated. */
RecompFunction recomp_d3d_miniport_lookup_manual(uint32_t guest_address);

/* Plain model of CDevice::KickOff with no GPU consumer: everything up to the
   current put is consumed at once, as D3D's own single-step path assumes. */
void recomp_d3d_miniport_kick_off(uint32_t device);

#endif
