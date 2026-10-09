#include "cri_adxm_adapter.h"
#include "kernel_abi.h"
#include "cri_service_model.h"
#include "d3d_vblank.h"
#include "xapi_time_adapter.h"

#include <stddef.h>

/* DOA3 addresses. ADXM_SetupThrd creates four threads: an idle spinner
   (0x0016A530), two vertical-blank threads (0x0016A570, 0x0016A5E0) and the
   main server (0x0016A650). One pass of each loop body: */
enum {
    VSYNC_SERVER_A = 0x00170640u,   /* first vertical-blank lane */
    VSYNC_SERVER_B = 0x00170660u,   /* second vertical-blank lane */
    MAIN_SERVER = 0x00170690u,      /* nonzero while work remains */
    MAIN_WAKE_FLAG = 0x00b24d3cu,   /* set to 1 to end a main-server batch */
    USER_CALLBACK = 0x00b24d50u,    /* optional (fn, arg) after each pass */
    USER_CALLBACK_ARG = 0x00b24d54u,
    /* ponytail: bounds a batch the guest never ends; raise it if a
       caller still sees pending work after one step. */
    MAIN_SERVER_LIMIT = 64u,
};

void recomp_cri_adxm_server_step(void)
{
    RecompRegisters saved = recomp_runtime.registers;
    uint32_t callback;

    kernel_call_guest(VSYNC_SERVER_A, NULL, 0u);
    kernel_call_guest(VSYNC_SERVER_B, NULL, 0u);
    for (unsigned i = 0u; i < MAIN_SERVER_LIMIT; ++i) {
        kernel_call_guest(MAIN_SERVER, NULL, 0u);
        if (recomp_runtime.registers.eax == 0u ||
            *recomp_memory_u32(MAIN_WAKE_FLAG) == 1u) {
            break;
        }
    }
    callback = *recomp_memory_u32(USER_CALLBACK);
    if (callback != 0u) {
        uint32_t argument = *recomp_memory_u32(USER_CALLBACK_ARG);
        kernel_call_guest(callback, &argument, 1u);
    }
    recomp_runtime.registers = saved;
}

/* The cooperative host can resume after several refreshes. Keep middleware
   time on the 60 Hz clock even when decoding or rendering misses a refresh. */
static void vblank_thread(uint32_t server, uint32_t stop)
{
    uint64_t tick = recomp_cri_vblank_tick(recomp_xapi_performance_counter());
    while (*recomp_memory_u32(stop) == 0u) {
        recomp_d3d_wait_vblank();
        uint64_t due = recomp_cri_vblank_tick(recomp_xapi_performance_counter());
        while (tick < due) {
            ++tick;
            ++*recomp_memory_u32(0x00b24d44u);
            kernel_call_guest(server, NULL, 0u);
        }
        uint32_t main_thread = *recomp_memory_u32(0x00c0c508u);
        kernel_call_guest(0x800000e0u, (uint32_t[]){main_thread, 0u}, 2u);
        uint32_t callback = *recomp_memory_u32(USER_CALLBACK);
        if (callback != 0u) {
            uint32_t argument = *recomp_memory_u32(USER_CALLBACK_ARG);
            kernel_call_guest(callback, &argument, 1u);
        }
    }
    *recomp_memory_u32(stop + 4u) = 1u;
    const uint32_t status = 0xf0000001u;
    kernel_call_guest(0x80000102u, &status, 1u);
}

void recomp_cri_adxm_vblank_a_thread(void) { vblank_thread(VSYNC_SERVER_A, 0x00b24d60u); }
void recomp_cri_adxm_vblank_b_thread(void) { vblank_thread(VSYNC_SERVER_B, 0x00b24d68u); }

/* The middleware worker owns a batch until the server drains or the caller
   requests an acknowledgement. Its wait is a real guest self-suspension. */
void recomp_cri_adxm_main_thread(void)
{
    const uint32_t self = 0xfffffffeu;
    const uint32_t suspend_args[2] = {self, 0u};
    while (*recomp_memory_u32(0x00b24d70u) == 0u) {
        ++*recomp_memory_u32(0x00b24d48u);
        kernel_call_guest(MAIN_SERVER, NULL, 0u);
        if (recomp_runtime.registers.eax != 0u &&
            *recomp_memory_u32(MAIN_WAKE_FLAG) != 1u) continue;
        if (*recomp_memory_u32(MAIN_WAKE_FLAG) == 1u) {
            uint32_t priority = *recomp_memory_u32(0x00b24d34u);
            /* Match XAPI's idle/time-critical saturation conversion. */
            if (priority == 15u) priority = 16u;
            if (priority == (uint32_t)-15) priority = (uint32_t)-16;
            const uint32_t args[2] = {self, priority};
            *recomp_memory_u32(MAIN_WAKE_FLAG) = 0u;
            kernel_call_guest(0x8000008fu, args, 2u); /* KeSetBasePriorityThread */
        }
        uint32_t callback = *recomp_memory_u32(USER_CALLBACK);
        if (callback != 0u) {
            uint32_t argument = *recomp_memory_u32(USER_CALLBACK_ARG);
            kernel_call_guest(callback, &argument, 1u);
        }
        kernel_call_guest(0x800000e7u, suspend_args, 2u); /* NtSuspendThread */
    }
    *recomp_memory_u32(0x00b24d74u) = 1u;
    const uint32_t status = 0xf0000001u;
    kernel_call_guest(0x80000102u, &status, 1u); /* PsTerminateSystemThread */
}
