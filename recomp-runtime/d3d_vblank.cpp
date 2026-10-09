#include "d3d_vblank.h"
#include "xapi_time_adapter.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <chrono>
#include <cstdint>
#include <thread>

extern "C" void recomp_kernel_wait_for_vblank(uint64_t deadline_ns);

namespace {
using Clock = std::chrono::steady_clock;
Clock::time_point last_vblank;
Clock::time_point completed_vblank;
Clock::time_point presented_vblank;
HANDLE high_resolution_timer;
// ponytail: the supported progressive NTSC mode; other video modes need their rate.
constexpr auto interval = std::chrono::nanoseconds(1000000000 / 60);
constexpr auto spin_window = std::chrono::microseconds(500);

Clock::time_point guest_now()
{
    return Clock::time_point(std::chrono::nanoseconds(recomp_xapi_performance_counter()));
}
}

void recomp_d3d_vblank_reset(void)
{
    last_vblank = guest_now();
    completed_vblank = last_vblank;
    presented_vblank = last_vblank;
}

void recomp_d3d_wait_vblank(void)
{
    // Every waiter in this refresh interval observes the same vblank.
    // Keep the deadline local across guest switches and nested waits.
    const auto now = guest_now();
    if (last_vblank <= now) {
        last_vblank += interval * ((now - last_vblank) / interval + 1);
    }
    const auto deadline = last_vblank;
    const auto deadline_ns = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            deadline.time_since_epoch()).count());
    recomp_kernel_wait_for_vblank(deadline_ns);
    if (recomp_xapi_skip_wait(deadline_ns)) {
        if (completed_vblank < deadline) completed_vblank = deadline;
        return;
    }
    if (high_resolution_timer == nullptr) {
        high_resolution_timer = CreateWaitableTimerExW(
            nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
            TIMER_MODIFY_STATE | SYNCHRONIZE);
    }
    if (high_resolution_timer == nullptr) {
        std::this_thread::sleep_until(deadline);
    } else {
        const auto spin_deadline = deadline - spin_window;
        // Deadlines are guest time, which runs ahead of host time after a skip.
        const auto timer_wait = spin_deadline - guest_now();
        if (timer_wait > Clock::duration::zero()) {
            LARGE_INTEGER due{};
            due.QuadPart = -std::chrono::duration_cast<std::chrono::nanoseconds>(
                timer_wait).count() / 100;
            if (due.QuadPart < 0 &&
                SetWaitableTimer(high_resolution_timer, &due, 0, nullptr, nullptr, FALSE)) {
                WaitForSingleObject(high_resolution_timer, INFINITE);
            } else {
                std::this_thread::sleep_until(deadline);
            }
        }
        while (guest_now() < deadline) YieldProcessor();
    }
    if (completed_vblank < deadline) completed_vblank = deadline;
}

void recomp_d3d_wait_present(void)
{
    // A guest that already waited this refresh may submit immediately.
    // Present-only loops still consume one new refresh per submission.
    if (completed_vblank <= presented_vblank) recomp_d3d_wait_vblank();
    presented_vblank = completed_vblank;
}
