#include "d3d_frame_adapter.h"
#include "d3d_draw_adapter.h"
#include "d3d_frame_model.h"
#include "d3d_vblank.h"
#include "d3d_presenter.h"
#include "d3d_render_state_adapter.h"
#include "d3d_texture_adapter.h"
#ifdef RECOMP_FULL_PROGRAM
#include "fiber_adapter.h"
#endif
#include "stop_report.h"
#include "save_transaction.h"
#include "xbox_memory_layout.h"

#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
#endif

/* DOA3 (D3D8 3925) addresses and CDevice fields; evidence in
   docs/doa3-d3d-frame.md. */
enum {
    D3D_DEVICE_CLEAR_ADDRESS = 0x001b3390u,
    D3D_DEVICE_PRESENT_ADDRESS = 0x001b5850u,
    D3D_DEVICE_SET_GAMMA_ADDRESS = 0x001b0e00u,
    D3D_DEVICE_RENDER_TARGET = 0x040cu,
    D3D_DEVICE_DEPTH_STENCIL = 0x0410u,
    /* Embedded surface objects, not pointers: GetBackBuffer(0) and the
       auto depth surface Reset passes to SetRenderTarget. */
    D3D_DEVICE_BACK_BUFFER = 0x2150u,
    D3D_DEVICE_AUTO_DEPTH = 0x219cu,
    /* Incremented by every guest Present; SetGammaRamp reads its low bit. */
    D3D_DEVICE_FRAME_COUNTER = 0x2b60u,
};

static RecompD3dFrameState frame_state;
static RecompD3dPresenter *presenter;
static uint32_t frame_device_address;
#ifdef _WIN32
static volatile LONG console_break_requested;

void recomp_d3d_frame_adapter_request_console_break(void)
{
    InterlockedExchange(&console_break_requested, 1);
}
#endif

static uint32_t stack_argument(uint32_t entry_esp, uint32_t index)
{
    return *recomp_memory_u32(entry_esp + 4u + index * 4u);
}

uint32_t recomp_d3d_frame_adapter_swap_counter(void)
{
    return frame_state.swap_counter;
}

void recomp_d3d_frame_adapter_initialize(
    const RecompD3dPresenterConfig *config,
    uint32_t device_address)
{
    RecompD3dFrameError frame_error;
    RecompD3dPresenterError presenter_error;

    if (config == NULL || device_address == 0u) {
        recomp_stop(2, "d3d-frame-init:device");
    }
    frame_error = recomp_d3d_frame_initialize(
        &frame_state, config->width, config->height);
    if (frame_error != RECOMP_D3D_FRAME_OK) {
        recomp_stop(2, "d3d-frame-init:model:%u", (unsigned)frame_error);
    }
    presenter_error = recomp_d3d_presenter_create(config, &presenter);
    if (presenter_error != RECOMP_D3D_PRESENTER_OK) {
        recomp_d3d_frame_reset(&frame_state);
        recomp_stop(
            2,
            "d3d-frame-init:presenter:%u",
            (unsigned)presenter_error);
    }
    frame_device_address = device_address;
    recomp_d3d_vblank_reset();
}

void recomp_d3d_frame_adapter_reset(void)
{
    if (presenter != NULL) {
        RecompD3dPresenterError error =
            recomp_d3d_presenter_destroy(&presenter);

        if (error != RECOMP_D3D_PRESENTER_OK) {
            recomp_stop(
                2,
                "d3d-frame-reset:presenter:%u",
                (unsigned)error);
        }
    }
    recomp_d3d_frame_reset(&frame_state);
    frame_device_address = 0u;
}

/* The window can close between frames, so any command may see CLOSED. */
void recomp_d3d_frame_adapter_exit_if_closed(RecompD3dPresenterError error)
{
    if (error == RECOMP_D3D_PRESENTER_CLOSED) {
        recomp_d3d_frame_adapter_reset();
        fprintf(stderr, "recomp runner: window closed; exiting normally\n");
        recomp_stop(0, "host-window-close");
    }
}

RecompD3dPresenter *recomp_d3d_frame_adapter_presenter(void)
{
    return presenter;
}

bool recomp_d3d_frame_adapter_target(RecompD3dPresenterTarget *target)
{
    uint32_t color;
    uint32_t back;
    uint32_t depth;
    uint32_t default_depth;

    if (target == NULL || frame_device_address == 0u) {
        return false;
    }
    *target = (RecompD3dPresenterTarget){0};
    color = *recomp_memory_u32(
        frame_device_address + D3D_DEVICE_RENDER_TARGET);
    back = frame_device_address + D3D_DEVICE_BACK_BUFFER;
    depth = *recomp_memory_u32(
        frame_device_address + D3D_DEVICE_DEPTH_STENCIL);
    default_depth = frame_device_address + D3D_DEVICE_AUTO_DEPTH;
    if (color == 0u) {
        goto reject;
    }
    target->offscreen = color != back;
    target->no_depth = depth == 0u;
    target->custom_depth = depth != 0u && depth != default_depth;
    if (target->offscreen &&
        (!recomp_d3d_texture_adapter_describe(color, &target->color) ||
         target->color.data == 0u || !target->color.render_target ||
         target->color.depth)) {
        goto reject;
    }
    if (target->custom_depth) {
        RecompD3dTextureDesc original = {0};

        if (!recomp_d3d_texture_adapter_describe(depth, &target->depth) ||
            target->depth.data == 0u || !target->depth.depth) {
            goto reject;
        }
        /* A surface wrapper can alias the default depth's pixel storage. */
        if (recomp_d3d_texture_adapter_describe(default_depth, &original) &&
            original.depth && original.data == target->depth.data &&
            original.format_byte == target->depth.format_byte &&
            original.width == target->depth.width &&
            original.height == target->depth.height &&
            original.linear == target->depth.linear &&
            original.pitch == target->depth.pitch) {
            target->custom_depth = false;
        }
    }
    return true;

reject:
    {
        static uint32_t reported;

        if (reported < 8u) {
            ++reported;
            fprintf(
                stderr,
                "recomp d3d target: rejected color=%08" PRIx32
                " back=%08" PRIx32 " data=%08" PRIx32
                " fmt=%02" PRIx32 " %ux%u depth=%08" PRIx32
                " default=%08" PRIx32 " data=%08" PRIx32
                " fmt=%02" PRIx32 " %ux%u\n",
                color, back, target->color.data, target->color.format_byte,
                target->color.width, target->color.height,
                depth, default_depth, target->depth.data,
                target->depth.format_byte,
                target->depth.width, target->depth.height);
        }
    }
    return false;
}

void recomp_d3d_frame_adapter_reset_buffers(void)
{
    RecompD3dFrameResult result =
        recomp_d3d_frame_reset_buffers(&frame_state);
    RecompD3dPresenterError presenter_error;

    if (result.error != RECOMP_D3D_FRAME_OK) {
        recomp_stop(
            2,
            "d3d-reset:model:%u",
            (unsigned)result.error);
    }
    presenter_error = recomp_d3d_presenter_submit(
        presenter, &result.command);
    recomp_d3d_frame_adapter_exit_if_closed(presenter_error);
    if (presenter_error != RECOMP_D3D_PRESENTER_OK) {
        recomp_stop(
            2,
            "d3d-reset:presenter:%u",
            (unsigned)presenter_error);
    }
}

void recomp_d3d_clear_adapter(void)
{
    uint32_t entry_esp = recomp_runtime.registers.esp;
    uint32_t saved_eax = recomp_runtime.registers.eax;
    RecompD3dFrameResult result = recomp_d3d_frame_clear(
        &frame_state,
        stack_argument(entry_esp, 0u),
        stack_argument(entry_esp, 1u),
        stack_argument(entry_esp, 2u),
        stack_argument(entry_esp, 3u),
        stack_argument(entry_esp, 4u),
        stack_argument(entry_esp, 5u));
    RecompD3dPresenterError presenter_error;

    if (result.error != RECOMP_D3D_FRAME_OK) {
        fprintf(
            stderr,
            "recomp d3d: Clear model rejected arguments (%u)\n",
            (unsigned)result.error);
        recomp_stop(2, "d3d-clear:model:%u", (unsigned)result.error);
    }
    if (!recomp_d3d_frame_adapter_target(&result.command.data.clear.target)) {
        recomp_stop(2, "d3d-clear:render-target");
    }
    presenter_error = recomp_d3d_presenter_submit(
        presenter, &result.command);
    recomp_d3d_frame_adapter_exit_if_closed(presenter_error);
    if (presenter_error != RECOMP_D3D_PRESENTER_OK) {
        fprintf(
            stderr,
            "recomp d3d: Clear presenter failed (%u)\n",
            (unsigned)presenter_error);
        recomp_stop(
            2,
            "d3d-clear:presenter:%u",
            (unsigned)presenter_error);
    }

    recomp_runtime.registers.eax = saved_eax;
    recomp_runtime.registers.esp = entry_esp + 28u;
}

/* Shared tail of every guest frame flip: validates the model result,
   paces, presents and mirrors the frame counter into the device. */
static void present_frame(uint32_t entry_esp, RecompD3dFrameResult result)
{
    RecompD3dPresenterError presenter_error;

    /* KeTickCount is exported to the guest as a data symbol at
       kKernelDataBase + 0x40 (runner.cpp), but nothing ever advanced it, so
       guest code that polls it saw time frozen at zero for the life of the
       process. The frozen native host runs a dedicated 1 ms thread for this
       (hostKeTickCountThreadProc), but a host thread writing guest memory
       asynchronously would make runs non-reproducible, and the camera is
       measurably bit-identical across runs today - a property worth keeping.

       Advance it from presentation instead: one swap is one displayed frame,
       so a fixed step per swap gives a monotonic millisecond counter that is
       deterministic and independent of host scheduling. 16 ms per swap is
       the NTSC frame interval this title targets.

       This is a kernel data export, so it is written here rather than in the
       swap model, which must stay free of host and kernel knowledge. */
    {
        enum {
            KE_TICK_COUNT_ADDRESS = XBOX_KERNEL_DATA_BASE + 0x40u,
            MILLISECONDS_PER_SWAP = 16u,
        };
        uint32_t *tick_count = recomp_memory_u32(KE_TICK_COUNT_ADDRESS);

        if (tick_count != NULL) {
            *tick_count += MILLISECONDS_PER_SWAP;
        }
    }

    /* Opt-in caller trace: RECOMP_D3D_WHOCALLS prints the dispatch chain
       that reached this present for the first 240 frames. */
    {
        static const char *who_trace;
        static bool who_trace_read;
        static uint32_t who_trace_lines;

        if (!who_trace_read) {
            who_trace_read = true;
            who_trace = getenv("RECOMP_D3D_WHOCALLS");
        }
        if (who_trace != NULL && who_trace_lines < 240u) {
            /* Generated calls push 0 in place of a return address, so walk
               the dispatch stack instead of the guest stack. */
            char chain[192];
            size_t used = 0u;
            size_t depth;

            chain[0] = '\0';
            for (depth = 0u; depth < 6u; ++depth) {
                uint32_t target = 0u;
                const char *member = NULL;
                int line = 0;
                int written;

                if (!recomp_dispatch_frame_at(
                        depth, &target, &member, &line)) {
                    break;
                }
                written = snprintf(
                    chain + used,
                    sizeof chain - used,
                    " %08" PRIx32 "@%s:%d",
                    target,
                    member != NULL ? member : "<adapter>",
                    line);
                if (written <= 0 || (size_t)written >= sizeof chain - used) {
                    break;
                }
                used += (size_t)written;
            }
            ++who_trace_lines;
            fprintf(
                stderr,
                "recomp d3d whocalls: frame=%lu esp=%08" PRIx32
                " chain=%s\n",
                (unsigned long)result.command.data.present.swap_counter,
                entry_esp,
                chain);
        }
    }

    if (result.error != RECOMP_D3D_FRAME_OK) {
        fprintf(
            stderr,
            "recomp d3d: Present model rejected arguments (%u)\n",
            (unsigned)result.error);
        recomp_stop(2, "d3d-present:model:%u", (unsigned)result.error);
    }
    /* Guest frame pacing advances once per present. Host VSync
       follows the monitor's refresh rate, not the guest's 60 Hz cadence. */
    recomp_d3d_wait_vblank();
    presenter_error = recomp_d3d_presenter_submit(
        presenter, &result.command);
    recomp_d3d_draw_adapter_capture_present(
        result.command.data.present.swap_counter, (uint32_t)presenter_error);
    recomp_d3d_frame_adapter_exit_if_closed(presenter_error);
    if (presenter_error != RECOMP_D3D_PRESENTER_OK) {
        fprintf(
            stderr,
            "recomp d3d: Present presenter failed (%u)\n",
            (unsigned)presenter_error);
        recomp_stop(
            2,
            "d3d-present:presenter:%u",
            (unsigned)presenter_error);
    }

#ifdef _WIN32
    if (InterlockedCompareExchange(&console_break_requested, 0, 0) != 0 &&
        !recomp_save_pending()) {
        recomp_d3d_frame_adapter_reset();
        fprintf(stderr, "recomp runner: console break; exiting normally\n");
        recomp_stop(0, "host-console-break");
    }
#endif

    *recomp_memory_u32(frame_device_address + D3D_DEVICE_FRAME_COUNTER) =
        result.command.data.present.swap_counter;
    /* Observation only: the draw seam reports transforms from inside a draw,
       but a run that never reaches DrawIndexedVertices then says nothing
       about whether the transform slots densified. Dump them from the swap
       path, which every run reaches, so the packed-SSE question is
       answerable independently of the draw. The first swap happens before
       any SetTransform, so this waits for a late swap. */
    {
        static bool xform_reported;
        const char *at_text = getenv("RECOMP_XFORM_DUMP_AT");
        unsigned long dump_at = at_text != NULL ? strtoul(at_text, NULL, 10) : 550ul;

        if (!xform_reported && frame_device_address != 0u &&
            (unsigned long)result.command.data.present.swap_counter >= dump_at) {
            xform_reported = true;
            for (uint32_t slot = 0u; slot < 7u; ++slot) {
                uint32_t address =
                    frame_device_address + 0x0810u + slot * 0x0040u;
                const uint32_t *m = recomp_memory_u32(address);
                float f[16];
                uint32_t nonzero = 0u;

                if (m == NULL) {
                    continue;
                }
                memcpy(f, m, sizeof f);
                for (uint32_t i = 0u; i < 16u; ++i) {
                    if (f[i] != 0.0f) {
                        ++nonzero;
                    }
                }
                fprintf(
                    stderr,
                    "recomp d3d swap-xform[%u] nonzero=%u "
                    "%g %g %g %g | %g %g %g %g | %g %g %g %g | %g %g %g %g\n",
                    slot, nonzero,
                    f[0], f[1], f[2], f[3], f[4], f[5], f[6], f[7],
                    f[8], f[9], f[10], f[11], f[12], f[13], f[14], f[15]);
            }
            fprintf(
                stderr,
                "recomp d3d swap-xform: draws submitted=%u declined=%u\n",
                (unsigned)recomp_d3d_draw_adapter_submitted(),
                (unsigned)recomp_d3d_draw_adapter_declined());
            recomp_d3d_draw_adapter_report_fvf();
            recomp_d3d_render_state_adapter_report();
            recomp_d3d_texture_adapter_report();
            recomp_d3d_presenter_report_draw_textures();
#ifdef RECOMP_FULL_PROGRAM
            recomp_fiber_adapter_report();
#endif
        }
    }
}

/* HRESULT __stdcall D3DDevice_Present(pSourceRect, pDestRect, pDummy1,
   pDummy2): ret 0x10. DOA3 calls it with four NULLs. */
void recomp_d3d_present_adapter(void)
{
    uint32_t entry_esp = recomp_runtime.registers.esp;

    present_frame(
        entry_esp,
        recomp_d3d_frame_present(
            &frame_state,
            stack_argument(entry_esp, 0u),
            stack_argument(entry_esp, 1u)));
    recomp_runtime.registers.eax = 0u; /* D3D_OK */
    recomp_runtime.registers.esp = entry_esp + 20u;
}

static void set_gamma_ramp(void)
{
    const uint32_t entry_esp = recomp_runtime.registers.esp;
    RecompD3dPresenterCommand command = {0};
    const uint32_t address = stack_argument(entry_esp, 1u);
    const uint8_t *ramp = recomp_memory(address, sizeof command.data.gamma);
    RecompD3dPresenterError error;

    if (address == 0u || ramp == NULL) {
        recomp_stop(2, "d3d-gamma:invalid-ramp");
    }
    command.type = RECOMP_D3D_PRESENTER_COMMAND_GAMMA;
    memcpy(command.data.gamma, ramp, sizeof command.data.gamma);
    error = recomp_d3d_presenter_submit(presenter, &command);
    recomp_d3d_frame_adapter_exit_if_closed(error);
    if (error != RECOMP_D3D_PRESENTER_OK) {
        recomp_stop(2, "d3d-gamma:presenter:%u", (unsigned)error);
    }
    recomp_runtime.registers.esp = entry_esp + 12u;
}

RecompFunction recomp_d3d_frame_lookup_manual(uint32_t guest_address)
{
    switch (guest_address) {
    case D3D_DEVICE_SET_GAMMA_ADDRESS:
        return set_gamma_ramp;
    case D3D_DEVICE_CLEAR_ADDRESS:
        return recomp_d3d_clear_adapter;
    case D3D_DEVICE_PRESENT_ADDRESS:
        return recomp_d3d_present_adapter;
    default:
        return NULL;
    }
}
