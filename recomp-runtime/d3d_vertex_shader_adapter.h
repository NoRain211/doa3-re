#ifndef DOAXBV_RECOMP_D3D_VERTEX_SHADER_ADAPTER_H
#define DOAXBV_RECOMP_D3D_VERTEX_SHADER_ADAPTER_H

#include "d3d_vertex_shader_model.h"
#include "runtime.h"

void recomp_d3d_set_vertex_shader_adapter(void);
RecompFunction recomp_d3d_vertex_shader_lookup_manual(uint32_t guest_address);

void recomp_d3d_vertex_shader_adapter_reset(void);
/* The vertex shader model, for a frame snapshot. */
size_t recomp_d3d_vertex_shader_adapter_state_size(void);
void recomp_d3d_vertex_shader_adapter_state_save(void *state);
void recomp_d3d_vertex_shader_adapter_state_restore(const void *state);
const RecompD3dVertexShaderModel *recomp_d3d_vertex_shader_adapter_model(void);

#endif
