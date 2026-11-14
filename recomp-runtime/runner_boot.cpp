/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "runner_boot.h"
#include "cri_service_adapter.h"
extern "C" {
#include "program_manual.h"
#include "doa3_build.h"
}
#include "widescreen.h"
#include "xapi_time_adapter.h"
#include "d3d_frame_adapter.h"
#include "fiber_adapter.h"
#include "free_camera_adapter.h"
#include "input_host_win32.h"
#include "perf_overlay.h"
#include "save_transaction.h"
#include "netplay_session.h"
#include "netplay_menu.h"
extern "C" {
#include "kernel_abi.h"
#include "native_fiber.h"
#include "overlay_text.h"
#include "quit_prompt.h"
}
#include "synctest.h"
#include "runtime.h"
#include "stop_report.h"
#include "xbox_memory_layout.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>

extern "C" void recomp_kernel_run_threads(void);
extern "C" RecompFunction recomp_program_lookup(std::uint32_t guest_address);

BootConfig bootConfig;

namespace {

// SyncTest replays frames, so the runner's own input schedule rewinds with them.
struct RunnerState {
    RecompInputPulseSource pulses;
    std::uint32_t inputFrame;
};

void saveRunnerState(void *state)
{
    *static_cast<RunnerState *>(state) = {inputPulseSource, inputFrame};
}

void restoreRunnerState(const void *state)
{
    const auto *in = static_cast<const RunnerState *>(state);
    inputPulseSource = in->pulses;
    inputFrame = in->inputFrame;
}

void onFrameBoundary(std::uint32_t frame)
{
    // A snapshot from before the main fiber exists cannot be restored.
    if (recomp_fiber_adapter_model()->current_handle == 0u) return;
    const RecompSyncTestResult result = recomp_synctest_frame(frame);
    if (result == RECOMP_SYNCTEST_MISMATCH) recomp_stop(2, "synctest:mismatch");
    if (result == RECOMP_SYNCTEST_SKIP) recomp_stop(3, "synctest:skip");
    if (result == RECOMP_SYNCTEST_MATCH && !recomp_synctest_loop())
        recomp_stop(0, "synctest:match");
}

// A in the prompt ends the run once no save is being written, as the console break does.
static bool quitChosen;

void quitPrompt(std::uint32_t port, RecompInputGamepad *gamepad)
{
    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    RecompOverlayText text;
    recomp_overlay_clear(&text);
    switch (recomp_quit_prompt_filter(port, gamepad, static_cast<std::uint64_t>(now))) {
    case RECOMP_QUIT_OPENED:
        recomp_overlay_set_title(&text, "Quit to the desktop?");
        recomp_overlay_set_hint(&text, "A quit   B keep playing");
        recomp_overlay_publish(RECOMP_OVERLAY_QUIT_PROMPT, &text);
        break;
    case RECOMP_QUIT_CANCELLED:
        quitChosen = false; // B after an A put off by a save keeps playing
        recomp_overlay_publish(RECOMP_OVERLAY_QUIT_PROMPT, &text);
        break;
    case RECOMP_QUIT_CHOSEN:
        quitChosen = true;
        break;
    case RECOMP_QUIT_NONE:
        break;
    }
    // The open prompt keeps the pads idle meanwhile.
    if (quitChosen && !recomp_save_pending()) recomp_stop(0, "player-quit");
}

// A sampled pad goes through the quit prompt, then the netplay menu. The open
// prompt owns the pads, so the room cannot inject its A behind it. An open
// netplay screen owns Back and Backspace (B, and deleting text), so holding
// either there starts no quit.
void filterHostPad(std::uint32_t port, RecompInputGamepad *gamepad, bool sampled)
{
    quitPrompt(port, sampled && !recomp_netplay_menu_open() ? gamepad : nullptr);
    if (sampled && !recomp_quit_prompt_open()) recomp_netplay_menu_filter(port, gamepad);
}

// A netplay session decides ports' pads once it runs; the runner's own source otherwise.
bool sampleSessionInput(std::uint32_t port, RecompInputGamepad *gamepad)
{
    bool present = false;
    if (recomp_netplay_session_pad(port, gamepad, &present)) return present;
    if (port == 0u && recomp_netplay_session_back_pad(gamepad)) return true;
    const bool sampled = bootConfig.inputSource(port, gamepad);
    filterHostPad(port, gamepad, sampled);
    return sampled;
}

#ifndef RECOMP_DOAXBV_BINDINGS
/* DOA3's widescreen mode holds 16:9 in data: its projection (9/16 height over
   width) and the fight camera's framing (16/9) and field of view. Another
   screen shape rewrites them so the game itself renders that shape. Wider
   than 16:9, the camera keeps its 16:9 framing and the view widens around
   it. The presenters fit the 2D layer and movies to the same shape. */
void setDoa3WidescreenShape(float aspect)
{
    const auto read = [](std::uint32_t address) {
        float value;
        std::memcpy(&value, recomp_memory(address, sizeof value), sizeof value);
        return value;
    };
    const auto write = [](std::uint32_t address, float value) {
        std::memcpy(recomp_memory(address, sizeof value), &value, sizeof value);
    };
    write(DOA3(0x001ed764u, 0x00251620u), 1.0f / aspect);
    const float framing = (std::min)(aspect, 16.0f / 9.0f);
    write(DOA3(0x001ed7c8u, 0x00251618u), framing);
    // The field of view narrows from 4:3 to 16:9; other shapes fall between.
    const float fov_4x3 = read(DOA3(0x001ed858u, 0x00251a70u));
    const float fov_16x9 = read(DOA3(0x001ed85cu, 0x00251a74u));
    const float wide = (framing - 4.0f / 3.0f) / (16.0f / 9.0f - 4.0f / 3.0f);
    write(DOA3(0x001ed85cu, 0x00251a74u), fov_4x3 + (fov_16x9 - fov_4x3) * wide);
}
#endif

VOID WINAPI rebootGuest(void *)
{
    recomp_reset_runtime_modules();
    bootGuest();
}

} // namespace

/* One boot of the game: the XBE mapped into fresh guest RAM, the runtime's
   per-boot state and hooks, then the guest itself. main runs the first; a
   room's session and the return from it run it again in this process. */
void bootGuest()
{
    // Per boot: a session keeps its saves apart (recomp_save_session).
    if (!recomp_save_initialize(recomp_disc_root_path)) {
        std::cerr << "recomp runner: save recovery failed; guest not started\n";
        std::exit(2);
    }
    const doaxbv::XbeMetadata &xbe = bootConfig.xbe.metadata;
    {
        std::vector<std::uint8_t> memory(kRamSize, 0u);
        std::string error;
        if (!mapXbe(bootConfig.xbe, memory, error)) {
            std::cerr << "recomp runner: " << error << '\n';
            std::exit(1);
        }
        std::memcpy(bootConfig.ram, memory.data(), kRamSize);
    }
    if (!xbox_HeapSetImageEnd(xbe.header.baseAddress + xbe.header.sizeImage)) {
        std::cerr << "recomp runner: invalid heap image boundary\n";
        std::exit(1);
    }
    static RecompMemoryRegion region;
    region = {0u, kRamSize, bootConfig.ram};
    recomp_runtime_init(&region, 1u, nullptr, 0u, nullptr, 0u);
    recomp_runtime_set_lookup(recomp_program_lookup);
    recomp_runtime.registers.esp = kStackTop;
    /* Both sides must compute the same frames from the same pads. */
    recomp_xapi_deterministic = bootConfig.deterministic || recomp_netplay_session_enabled();
    recomp_xapi_fast_forward = recomp_doa3_boot_fast_forward;
    recomp_cri_service_adapter_reset();
    recomp_fiber_adapter_reset();
    recomp_input_adapter_reset();
#ifdef RECOMP_DOAXBV_BINDINGS
    /* DOAXBV's options defaults table turns Vibration off; default it on. */
    *recomp_memory(0x003a4c35u, 1u) = 1u;
#else
    /* DOA3 reads the stick only with Analog Button on, but its options
       defaults table turns it off; default it on. Saves keep their choice. */
    *recomp_memory(DOA3(0x0021b4fbu, 0x003f4563u), 1u) = 1u;
    unsigned aspect_w = 0u, aspect_h = 0u;
    if (recomp_widescreen(&aspect_w, &aspect_h)) {
        setDoa3WidescreenShape(static_cast<float>(aspect_w) / aspect_h);
    }
#endif
    // Each boot sets every hook, so nothing from a session that ended carries over.
    recomp_d3d_frame_boundary_hook = nullptr;
    recomp_kernel_profile_mutation_hook = nullptr;
    if (recomp_synctest_enabled() || recomp_netplay_session_enabled()) {
        recomp_synctest_set_guest_ram(bootConfig.ram, kRamSize, bootConfig.ramWriteWatch);
        recomp_synctest_install_default_modules();
        if (!recomp_netplay_session_enabled()) {
            recomp_synctest_set_extra_state(sizeof(RunnerState), saveRunnerState, restoreRunnerState);
            recomp_d3d_frame_boundary_hook = onFrameBoundary;
        }
    }
    // The session hands out each frame's pads; its own runs keep the runner's source underneath.
    recomp_input_adapter_set_source(sampleSessionInput);
    recomp_input_adapter_set_feedback_sink(bootConfig.scriptedInput
        ? recomp_input_host_set_vibration : recomp_netplay_session_set_vibration);
    recomp_d3d_overlay_hooks[RECOMP_D3D_OVERLAY_PERF] = recomp_perf_overlay;
    recomp_netplay_session_install(recomp_synctest_enabled());
    recomp_d3d_present_hook = onPresent;
    /* The free camera and the scene freeze run only with RECOMP_FREE_CAMERA=1,
       and never online; off, input samples and task switches go unhooked. */
    recomp_free_camera_install(recomp_netplay_session_enabled() || recomp_synctest_enabled());

    if (!recomp_dispatch(xbe.selectedEntryPoint)) {
        std::cerr << "recomp runner: entry point is not registered\n";
        std::exit(2);
    }
    recomp_kernel_run_threads();
    std::cout << "entry=" << doaxbv::hex32(xbe.selectedEntryPoint)
              << " sha256=" << xbe.fileSha256 << '\n';
    /* Reaching here means the entry point returned rather than stopping at a
       named boundary. Route it through the same reporter so an expectation of
       "completed" can be asserted and the milestone log stays complete. */
    recomp_stop(0, "completed");
}

void reboot()
{
    recomp_input_host_stop_vibration();
    // The old boot's stacks are never resumed; the reset frees them from the new fiber.
    void *fiber = recomp_native_fiber_create(rebootGuest, nullptr);
    if (fiber == nullptr) recomp_stop(2, "runner:reboot-fiber");
    recomp_native_fiber_switch(fiber);
}
