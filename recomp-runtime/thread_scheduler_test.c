#include "kernel_abi.h"
#include "fiber_adapter.h"
#include "d3d_vblank.h"
#include "d3d_frame_adapter.h"
#include "cri_adxm_adapter.h"
#include "xbox_memory_layout.h"
#include "xapi_time_adapter.h"

#include <stdio.h>
#include <string.h>

enum { WORKER = 0x123400u, FIBER = 0x123410u, APC = 0x123420u,
       VBLANK_WORKER = 0x123440u, PRIORITY_WORKER = 0x123430u, TIMER_DPC_ROUTINE = 0x123450u,
       STATE = 0x2000u, TIMEOUT = 0x2100u, MAIN_ESP = 0x8000u };
static uint32_t events[2], handles[2], turns[2], apcs[2], main_fibers[2], starts;
static int passed = 1;
static uint32_t priority_steps;
static uint32_t cri_passes;
static uint32_t timer_dpcs;
static uint32_t vblank_steps;
static uint64_t vblank_deadline;
void recomp_test_heap_reset(uint32_t cursor, int fail_after);

static void check(const char *name, uint64_t actual, uint64_t expected)
{
    if (actual != expected) {
        fprintf(stderr, "thread scheduler: %s was %llu, expected %llu\n",
                name, (unsigned long long)actual, (unsigned long long)expected);
        passed = 0;
    }
}

static uint32_t call(RecompFunction function, unsigned count, const uint32_t *args)
{
    uint32_t esp = recomp_runtime.registers.esp;
    for (unsigned i = count; i != 0u; --i) {
        recomp_runtime.registers.esp -= 4u;
        *recomp_memory_u32(recomp_runtime.registers.esp) = args[i - 1u];
    }
    recomp_runtime.registers.esp -= 4u;
    *recomp_memory_u32(recomp_runtime.registers.esp) = 0u;
    function();
    check("callee stack cleanup", recomp_runtime.registers.esp, esp);
    return recomp_runtime.registers.eax;
}

#define KERNEL(ord, ...) call(recomp_kernel_thread(ord), \
    sizeof((uint32_t[]){__VA_ARGS__}) / sizeof(uint32_t), (uint32_t[]){__VA_ARGS__})

static uint32_t fiber_call(uint32_t entry, unsigned count, const uint32_t *args)
{
    return call(recomp_fiber_lookup_manual(entry, &recomp_fiber_doa3_bindings), count, args);
}

static void apc(void)
{
    uint32_t index = kernel_arg(1u);
    check("APC thread", recomp_kernel_current_thread_id(), handles[index]);
    ++apcs[index];
    kernel_return(3u, 0u);
}

static void ping_pong(void)
{
    uint32_t index = kernel_arg(1u);
    uint32_t object = *recomp_memory_u32(0x28u);
    uint32_t tls = *recomp_memory_u32(4u);
    for (uint32_t turn = 0u; turn < 3u; ++turn) {
        check("event wait", KERNEL(234u, events[index], 1u, 0u, 0u), 0u);
        check("other APC stays queued", apcs[index], turn);
        recomp_runtime.registers.ebx = 0x11110000u + index;
        recomp_runtime.mmx[0].q = 0x1234567800000000ull + index;
        recomp_runtime.fpu_status_cc = (uint16_t)(index + 1u);
        recomp_runtime.direction_flag = (int)index;
        recomp_runtime.arithmetic_flags = 0x40u * index;
        *recomp_memory_u32(0u) = 0xaabb0000u + index;
        recomp_kernel_queue_user_apc(APC, index, 0u);
        uint64_t before = recomp_xapi_performance_counter();
        check("delay", KERNEL(99u, 1u, 0u, TIMEOUT), 0u);
        check("delay elapsed", recomp_xapi_performance_counter() - before >=
            recomp_xapi_performance_frequency() / 1000u, 1u);
        check("thread identity", recomp_kernel_current_thread_id(), handles[index]);
        check("KTHREAD", *recomp_memory_u32(0x28u), object);
        check("TLS", *recomp_memory_u32(4u), tls);
        check("SEH", *recomp_memory_u32(0u), 0xaabb0000u + index);
        check("register", recomp_runtime.registers.ebx, 0x11110000u + index);
        check("MMX", recomp_runtime.mmx[0].q, 0x1234567800000000ull + index);
        check("FP flags", recomp_runtime.fpu_status_cc, index + 1u);
        check("direction", recomp_runtime.direction_flag, index);
        check("arithmetic flags", recomp_runtime.arithmetic_flags, 0x40u * index);
        check("alertable APC", KERNEL(99u, 1u, 1u, TIMEOUT), 0xc0u);
        check("APC count", apcs[index], turn + 1u);
        ++turns[index];
        check("set event", KERNEL(225u, events[1u - index], 0u), 0u);
    }
    if (index == 0u) {
        check("self suspend", KERNEL(231u, 0xfffffffeu, 0u), 0u);
    } else {
        check("resume peer", KERNEL(224u, handles[0], 0u), 0u);
    }
    uint32_t target = main_fibers[index];
    fiber_call(recomp_fiber_doa3_bindings.switch_to, 1u, &target);
    check("completed fiber resumed", 1u, 0u);
}

static void worker(void)
{
    ++starts;
    uint32_t index = kernel_arg(1u);
    uint32_t tls_slot = *recomp_memory_u32(*recomp_memory_u32(0x28u) + 0x28u);
    /* XAPI's system startup initializes the TLS slot before game code runs. */
    *recomp_memory_u32(tls_slot) = tls_slot + 4u;
    check("TLS stack top", *recomp_memory_u32(4u), tls_slot + 0x40u);
    check("KTHREAD ID", *recomp_memory_u32(*recomp_memory_u32(0x28u) + 0x12cu), handles[index]);
    uint32_t data = index;
    main_fibers[index] = fiber_call(recomp_fiber_doa3_bindings.convert_thread, 1u, &data);
    uint32_t args[] = {0x4000u, FIBER, index};
    uint32_t fiber = fiber_call(recomp_fiber_doa3_bindings.create, 3u, args);
    fiber_call(recomp_fiber_doa3_bindings.switch_to, 1u, &fiber);
    check("restored main fiber", recomp_fiber_adapter_model()->current_handle, main_fibers[index]);
    fiber_call(recomp_fiber_doa3_bindings.delete_fiber, 1u, &fiber);
    KERNEL(258u, 0u);
}

static void priority_worker(void)
{
    for (unsigned i = 0u; i < 4u; ++i) {
        ++priority_steps;
        if (i != 3u) KERNEL(231u, 0xfffffffeu, 0u);
    }
}

static void vblank_worker(void)
{
    ++vblank_steps;
    recomp_kernel_wait_for_vblank(vblank_deadline);
    ++vblank_steps;
}

static void cri_server_probe(void)
{
    ++cri_passes;
    kernel_return(0u, 0u);
}

static void timer_dpc(void)
{
    ++timer_dpcs;
    kernel_return(4u, 0u);
}

static RecompFunction kernel_lookup(uint32_t address)
{
    return (address & 0xffff0000u) == 0x80000000u
        ? recomp_kernel_thread(address & 0xffffu) : NULL;
}

int recomp_thread_scheduler_test(void)
{
    static uint8_t ram[RECOMP_XBOX_RAM_SIZE];
    const RecompMemoryRegion region = {0u, sizeof ram, ram};
    const RecompFunctionEntry functions[] = {{WORKER, worker}, {FIBER, ping_pong}, {APC, apc}, {PRIORITY_WORKER, priority_worker},
        {VBLANK_WORKER, vblank_worker}, {0x0016a650u, recomp_cri_adxm_main_thread}, {0x00170690u, cri_server_probe},
        {TIMER_DPC_ROUTINE, timer_dpc}};
    recomp_runtime_init(&region, 1u, NULL, 0u, functions, sizeof functions / sizeof functions[0]);
    recomp_runtime_set_lookup(kernel_lookup);
    recomp_test_heap_reset(0x1000000u, -1);
    recomp_runtime.registers.esp = MAIN_ESP;
    *recomp_memory_u32(0u) = 0xffffffffu;
    *recomp_memory_u32(0x28u) = XBOX_STARTUP_THREAD_OBJECT;
    *recomp_memory_u32(recomp_fiber_doa3_bindings.tls_index) = (uint32_t)-16;
    *recomp_memory_u64(TIMEOUT) = (uint64_t)(int64_t)-10000; /* 1 ms */
    for (unsigned i = 0u; i < 2u; ++i) {
        check("create event", KERNEL(189u, STATE, 0u, 1u, i == 0u), 0u);
        events[i] = *recomp_memory_u32(STATE);
        check("create suspended thread", KERNEL(255u, STATE, 0u, 0x4000u,
            0x40u, 0u, WORKER, i, 1u, 0u, 0u), 0u);
        handles[i] = *recomp_memory_u32(STATE);
    }
    call(recomp_kernel_thread(238u), 0u, NULL);
    check("suspended threads did not run", turns[0] + turns[1], 0u);
    check("suspend again", KERNEL(231u, handles[0], STATE), 0u);
    check("previous suspend", *recomp_memory_u32(STATE), 1u);
    KERNEL(224u, handles[0], STATE);
    check("previous resume", *recomp_memory_u32(STATE), 2u);
    for (unsigned i = 0u; i < 2u; ++i) {
        KERNEL(143u, handles[i], i + 2u);
        check("per-thread priority", KERNEL(124u, handles[i]), i + 2u);
        KERNEL(143u, handles[i], 0u);
        KERNEL(224u, handles[i], STATE);
        check("final resume", *recomp_memory_u32(STATE), 1u);
    }
    /* WaitAll must not consume a partial set of synchronization events. */
    *recomp_memory_u32(STATE) = events[0];
    *recomp_memory_u32(STATE + 4u) = events[1];
    *recomp_memory_u64(TIMEOUT + 8u) = 0u;
    check("WaitAll poll", KERNEL(235u, 2u, STATE, 0u, 1u, 0u, TIMEOUT + 8u), 0x102u);
    check("poll did not switch", turns[0] + turns[1], 0u);
    for (unsigned i = 0u; i < 2u; ++i) *recomp_memory_u32(STATE + i * 4u) = handles[i];
    recomp_d3d_vblank_reset();
    call(recomp_d3d_frame_lookup_manual(0x001b1130u), 0u, NULL);
    check("vblank scheduled both ready workers", starts, 2u);
    recomp_kernel_run_threads();
    check("join workers", KERNEL(235u, 2u, STATE, 0u, 1u, 0u, 0u), 0u);
    check("ping rounds", turns[0], 3u);
    check("pong rounds", turns[1], 3u);
    check("main identity", recomp_kernel_current_thread_id(), 1u);
    check("main SEH", *recomp_memory_u32(0u), 0xffffffffu);
    check("main fiber context", recomp_fiber_adapter_model()->current_handle, 0u);
    check("join already exited", KERNEL(233u, handles[0], 0u, 0u), 0u);
    /* A synchronization timer fires once; a poll after consumption times out. */
    KERNEL(113u, STATE + 0x40u, 1u);
    KERNEL(149u, STATE + 0x40u, (uint32_t)-10000, 0xffffffffu, 0u);
    check("timer wait", KERNEL(159u, STATE + 0x40u, 0u, 0u, 0u, 0u), 0u);
    check("timer consumed", KERNEL(159u, STATE + 0x40u, 0u, 0u, 0u, TIMEOUT + 8u), 0x102u);
    /* A timer DPC runs at expiry, not when the timer is set (50 ms ahead). */
    KERNEL(113u, STATE + 0x80u, 1u);
    *recomp_memory_u32(STATE + 0xc0u + 0x0cu) = TIMER_DPC_ROUTINE;
    check("timer newly set", KERNEL(149u, STATE + 0x80u, (uint32_t)-500000, 0xffffffffu, STATE + 0xc0u), 0u);
    recomp_kernel_drain_dpcs();
    check("timer DPC waits for due time", timer_dpcs, 0u);
    check("timer DPC wait", KERNEL(159u, STATE + 0x80u, 0u, 0u, 0u, 0u), 0u);
    recomp_kernel_drain_dpcs();
    check("timer DPC ran once at expiry", timer_dpcs, 1u);
    /* Re-arming an overdue timer fires it first, as its clock interrupt would have. */
    KERNEL(149u, STATE + 0x80u, (uint32_t)-10000, 0xffffffffu, STATE + 0xc0u);
    for (uint64_t until = recomp_xapi_performance_counter() +
            recomp_xapi_performance_frequency() / 200u;
         recomp_xapi_performance_counter() < until;) {}
    check("overdue timer was not still set",
        KERNEL(149u, STATE + 0x80u, (uint32_t)-500000, 0xffffffffu, STATE + 0xc0u), 0u);
    recomp_kernel_drain_dpcs();
    check("overdue timer DPC ran before re-arm", timer_dpcs, 2u);
    KERNEL(97u, STATE + 0x80u);
    check("create priority worker", KERNEL(255u, STATE, 0u, 0x4000u,
        0u, 0u, PRIORITY_WORKER, 0u, 1u, 0u, 0u), 0u);
    uint32_t priority_handle = *recomp_memory_u32(STATE);
    KERNEL(143u, priority_handle, 1u);
    check("raising suspended priority does not run", priority_steps, 0u);
    KERNEL(224u, priority_handle, 0u);
    check("NtResumeThread preempts", priority_steps, 1u);
    KERNEL(143u, priority_handle, 0u);
    KERNEL(224u, priority_handle, 0u);
    check("equal priority does not preempt", priority_steps, 1u);
    KERNEL(143u, priority_handle, 2u);
    check("base priority raise preempts", priority_steps, 2u);
    KERNEL(143u, priority_handle, 0u);
    KERNEL(224u, priority_handle, 0u);
    check("absolute priority previous", KERNEL(148u, priority_handle, 9u), 8u);
    check("absolute priority raise preempts", priority_steps, 3u);
    check("KeResumeThread previous count", KERNEL(140u, priority_handle), 1u);
    check("KeResumeThread preempts", priority_steps, 4u);
    KERNEL(255u, STATE, 0u, 0x4000u, 0u, 0u, VBLANK_WORKER, 0u, 1u, 0u, 0u);
    uint32_t vblank_handle = *recomp_memory_u32(STATE);
    vblank_deadline = recomp_xapi_performance_counter() + 50000000u;
    KERNEL(143u, vblank_handle, 1u);
    KERNEL(224u, vblank_handle, 0u);
    check("vblank worker entered", vblank_steps, 1u);
    KERNEL(143u, vblank_handle, 2u);
    if (recomp_xapi_performance_counter() < vblank_deadline)
        check("priority cannot wake a vblank waiter", vblank_steps, 1u);
    check("vblank worker join", KERNEL(233u, vblank_handle, 0u, 0u), 0u);
    check("vblank worker completed", vblank_steps, 2u);
    *recomp_memory_u32(0x00b24d3cu) = 1u;
    KERNEL(255u, STATE, 0u, 0x4000u, 0u, 0u, 0x0016a650u, 0u, 1u, 0u, 0u);
    uint32_t cri = *recomp_memory_u32(STATE);
    KERNEL(143u, cri, 1u);
    KERNEL(224u, cri, 0u);
    check("CRI server pass before acknowledgement", cri_passes, 1u);
    check("CRI request acknowledged", *recomp_memory_u32(0x00b24d3cu), 0u);
    *recomp_memory_u32(0x00b24d70u) = 1u;
    KERNEL(143u, cri, 1u);
    KERNEL(224u, cri, 0u);
    check("CRI worker exited", KERNEL(233u, cri, 0u, 0u), 0u);
    check("CRI exit acknowledged", *recomp_memory_u32(0x00b24d74u), 1u);
    return passed;
}
