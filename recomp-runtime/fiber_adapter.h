#ifndef DOAXBV_RECOMP_FIBER_ADAPTER_H
#define DOAXBV_RECOMP_FIBER_ADAPTER_H

#include "fiber_model.h"
#include "runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RecompFiberBindings {
    uint32_t create;
    uint32_t delete_fiber;
    uint32_t switch_to;
    uint32_t convert_thread;
    uint32_t tls_index;
} RecompFiberBindings;

extern const RecompFiberBindings recomp_fiber_doa3_bindings;
extern const RecompFiberBindings recomp_fiber_doaxbv_bindings;
RecompFunction recomp_fiber_lookup_manual(
    uint32_t guest_address, const RecompFiberBindings *bindings);
uint32_t recomp_fiber_thread_context(void);
void recomp_fiber_thread_restore(uint32_t handle);
void recomp_fiber_adapter_reset(void);
const RecompFiberModel *recomp_fiber_adapter_model(void);
void recomp_fiber_adapter_report(void);

#ifdef __cplusplus
}
#endif

#endif
