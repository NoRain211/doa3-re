#ifndef DOAXBV_RECOMP_D3D_CREATION_ADAPTER_H
#define DOAXBV_RECOMP_D3D_CREATION_ADAPTER_H

#include "runtime.h"
#include "synctest.h"

void recomp_d3d_create_device_adapter(void);
void recomp_d3d_reset_device_adapter(void);
void recomp_d3d_persist_display_adapter(void);
void recomp_d3d_make_requested_space_adapter(void);
void recomp_d3d_kick_off_adapter(void);
RecompFunction recomp_d3d_lookup_manual(uint32_t guest_address);

/* Test seam: reset the file-static creation model between adapter runs. */
void recomp_d3d_creation_adapter_reset(void);
/* The same for a reboot of the game: the presenter and its window stay. */
void recomp_d3d_creation_adapter_reset_keeping_window(void);
/* The creation model, for a frame snapshot. */
extern const RecompSyncTestModule recomp_d3d_creation_adapter_module;

#endif
