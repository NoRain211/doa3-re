#ifndef DOAXBV_RECOMP_D3D_TEXTURE_ADAPTER_H
#define DOAXBV_RECOMP_D3D_TEXTURE_ADAPTER_H

#include "d3d_texture_model.h"
#include "runtime.h"

RecompFunction recomp_d3d_texture_lookup_manual(uint32_t guest_address);

void recomp_d3d_texture_adapter_reset(void);
const RecompD3dTextureModel *recomp_d3d_texture_adapter_model(void);
const RecompD3dTextureDesc *recomp_d3d_texture_adapter_stage(uint32_t stage);
bool recomp_d3d_texture_adapter_describe(
    uint32_t resource, RecompD3dTextureDesc *out);
void recomp_d3d_texture_adapter_report(void);
/* The texture model and stage descriptions, for a frame snapshot. */
size_t recomp_d3d_texture_adapter_state_size(void);
void recomp_d3d_texture_adapter_state_save(void *state);
void recomp_d3d_texture_adapter_state_restore(const void *state);

#endif
