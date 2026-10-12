#include "xapi_time_adapter.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>

namespace {
using Clock = std::chrono::steady_clock;
std::atomic<uint64_t> skipped_ns{0};
// Deterministic mode: guest time is this counter alone, moved only by skips.
std::atomic<uint64_t> virtual_ns{0};
Clock::time_point pace_target;
bool pace_started;

// Host pacing only: never feeds back into guest time.
void pace(uint64_t advance_ns)
{
    static HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr,
        CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_MODIFY_STATE | SYNCHRONIZE);
    const auto now = Clock::now();
    // A host stall forfeits the lost time instead of racing to catch up.
    if (!pace_started || pace_target + std::chrono::milliseconds(100) < now) {
        pace_target = now;
        pace_started = true;
    }
    pace_target += std::chrono::nanoseconds(advance_ns);
    const auto sleep = pace_target - std::chrono::microseconds(500) - now;
    if (timer != nullptr && sleep > Clock::duration::zero()) {
        LARGE_INTEGER due{};
        due.QuadPart = -std::chrono::duration_cast<std::chrono::nanoseconds>(sleep).count() / 100;
        if (due.QuadPart < 0 && SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE)) {
            WaitForSingleObject(timer, INFINITE);
        }
    }
    while (Clock::now() < pace_target) YieldProcessor();
}
}

bool (*recomp_xapi_fast_forward)(void) = nullptr;
bool recomp_xapi_deterministic = false;
uint64_t recomp_xapi_clock_reads;

uint64_t recomp_xapi_performance_counter(void)
{
    if (recomp_xapi_deterministic) {
        ++recomp_xapi_clock_reads;
        return virtual_ns.load(std::memory_order_relaxed);
    }
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            Clock::now().time_since_epoch()).count()) +
        skipped_ns.load(std::memory_order_relaxed);
}

uint64_t recomp_xapi_performance_frequency(void)
{
    return UINT64_C(1000000000);
}

uint64_t xbox_ReadTimeStampCounter(void)
{
    const uint64_t now = recomp_xapi_performance_counter();
    const uint64_t frequency = recomp_xapi_performance_frequency();
    // 733,333,333 Hz, split to avoid overflowing the product.
    return now / frequency * 733333333u + now % frequency * 733333333u / frequency;
}

bool recomp_xapi_skip_wait(uint64_t deadline_ns)
{
    static const bool unpaced = [] {
        const char *value = std::getenv("RECOMP_UNPACED");
        return value != nullptr && std::strcmp(value, "1") == 0;
    }();
    const bool skip = unpaced || (recomp_xapi_fast_forward != nullptr && recomp_xapi_fast_forward());
    if (recomp_xapi_deterministic) {
        const uint64_t now = virtual_ns.load(std::memory_order_relaxed);
        if (deadline_ns <= now) return true;
        virtual_ns.store(deadline_ns, std::memory_order_relaxed);
        if (!skip) pace(deadline_ns - now);
        return true;
    }
    if (!skip)
        return false;
    // Only the guest execution thread advances time, after runnable work yields.
    const uint64_t now = recomp_xapi_performance_counter();
    if (deadline_ns > now)
        skipped_ns.fetch_add(deadline_ns - now, std::memory_order_relaxed);
    return true;
}
