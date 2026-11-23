/* Europe 3.2 (XDK 4134), generated from program_forwards_jp.c with the
   Japan-to-Europe function map; see docs/doa3-eur-test.md. The table keys
   are Europe addresses and still map to the USA adapter keys. */
/* Library entry points rebound to hand-written adapters.

   The lifter generates no body for an address listed in
   tools/doa3/manual_functions.json and emits every call to it as indirect
   dispatch, which tries recomp_lookup_manual() first. The dispatch table and
   recomp_funcs.h still name sub_X, so define it here as one
   recomp_program_manual_call() call, e.g.
     void sub_001DF950(void) { recomp_program_manual_call(0x001df950u); }
   It runs only when no adapter is bound, and then stops naming the address. */
#include "program_manual.h"
#include "stop_report.h"
#include "cri_adxm_adapter.h"
#include "kernel_abi.h"
#include "free_camera_adapter.h"
#include "menu_quit.h"

#include <inttypes.h>
#include <stddef.h>

void recomp_program_manual_call(uint32_t guest_address)
{
    RecompFunction function = recomp_lookup_manual(guest_address);

    if (function == NULL) {
        recomp_stop(2, "manual-unbound:0x%08" PRIx32, guest_address);
        return;
    }
    function();
}

/* FIBER: DOA3 XAPILIB 3911; adapters in fiber_adapter.c. */
void sub_0018D2A4(void) { recomp_program_manual_call(0x0018d2a4u); }
void sub_0018D330(void) { recomp_program_manual_call(0x0018d330u); }
void sub_0018D343(void) { recomp_program_manual_call(0x0018d343u); }
void sub_0018D382(void) { recomp_program_manual_call(0x0018d382u); }
/* FIBER end. */

/* D3DDevice_SetGammaRamp, D3DDevice_Clear, D3DDevice_Present (XDK 3925).
   Their adapters are in d3d_frame_adapter.c; see docs/doa3-d3d-symbols.md. */
void sub_001DCCA0(void) { recomp_program_manual_call(0x001dcca0u); }
void sub_001DF950(void) { recomp_program_manual_call(0x001df950u); }
void sub_001DCFA0(void) { recomp_program_manual_call(0x001dcfa0u); }
void sub_001E1E10(void) { recomp_program_manual_call(0x001e1e10u); }

/* D3D8 4134 draw, texture, vertex shader and render-state setters; adapters
   in d3d_draw_adapter.c, d3d_texture_adapter.c, d3d_vertex_shader_adapter.c
   and d3d_render_state_adapter.c. */
void sub_001DDFB0(void) { recomp_program_manual_call(0x001ddfb0u); }
void sub_001DE1C0(void) { recomp_program_manual_call(0x001de1c0u); }
void sub_001DE7F0(void) { recomp_program_manual_call(0x001de7f0u); }
void sub_001DE9C0(void) { recomp_program_manual_call(0x001de9c0u); }
void sub_001DEA90(void) { recomp_program_manual_call(0x001dea90u); }
void sub_001DEB40(void) { recomp_program_manual_call(0x001deb40u); }
void sub_001DEB80(void) { recomp_program_manual_call(0x001deb80u); }
void sub_001DEDB0(void) { recomp_program_manual_call(0x001dedb0u); }
void sub_001DF580(void) { recomp_program_manual_call(0x001df580u); }
void sub_001DF620(void) { recomp_program_manual_call(0x001df620u); }
void sub_001DF6B0(void) { recomp_program_manual_call(0x001df6b0u); }
void sub_001DF890(void) { recomp_program_manual_call(0x001df890u); }
void sub_001DFDD0(void) { recomp_program_manual_call(0x001dfdd0u); }
void sub_001DFF30(void) { recomp_program_manual_call(0x001dff30u); }
void sub_001DFFD0(void) { recomp_program_manual_call(0x001dffd0u); }
void sub_001E0890(void) { recomp_program_manual_call(0x001e0890u); }
void sub_001E0C50(void) { recomp_program_manual_call(0x001e0c50u); }

/* XInitDevices and its jmp thunk, XGetDevices, XGetDeviceChanges, XInputOpen,
   XInputClose, XInputGetCapabilities, XInputGetState, XInputSetState
   (XAPILIB 3911, XPP section). Adapters are in input_adapter.c. */
void sub_002114AC(void) { recomp_program_manual_call(0x002114acu); }
void sub_00211EB8(void) { recomp_program_manual_call(0x00211eb8u); }
void sub_00211EBD(void) { recomp_program_manual_call(0x00211ebdu); }
void sub_00211EDF(void) { recomp_program_manual_call(0x00211edfu); }
void sub_00212163(void) { recomp_program_manual_call(0x00212163u); }
void sub_002121D8(void) { recomp_program_manual_call(0x002121d8u); }
void sub_002121E4(void) { recomp_program_manual_call(0x002121e4u); }
void sub_002123D6(void) { recomp_program_manual_call(0x002123d6u); }
void sub_00212447(void) { recomp_program_manual_call(0x00212447u); }

/* CDevice::KickOff and the miniport GPU layer under CDevice::Init (D3D8 3925).
   Adapters are in d3d_miniport_adapter.c. */
void sub_001E3100(void) { recomp_program_manual_call(0x001e3100u); }
void sub_001E612D(void) { recomp_program_manual_call(0x001e612du); }
void sub_001E5BC3(void) { recomp_program_manual_call(0x001e5bc3u); }
void sub_001E5C26(void) { recomp_program_manual_call(0x001e5c26u); }
void sub_001E5F63(void) { recomp_program_manual_call(0x001e5f63u); }
void sub_001E6032(void) { recomp_program_manual_call(0x001e6032u); }
void sub_001E5098(void) { recomp_program_manual_call(0x001e5098u); }
void sub_001E628F(void) { recomp_program_manual_call(0x001e628fu); }
void sub_001E647D(void) { recomp_program_manual_call(0x001e647du); }
void sub_001E30D0(void) { recomp_program_manual_call(0x001e30d0u); }

/* DirectSound 3936 wrappers game code calls (dsound_api_adapter.c). */
void sub_001F37A5(void) { recomp_program_manual_call(0x001f37a5u); }
void sub_001F2841(void) { recomp_program_manual_call(0x001f2841u); }
void sub_001F2686(void) { recomp_program_manual_call(0x001f2686u); }
void sub_001F26A2(void) { recomp_program_manual_call(0x001f26a2u); }
void sub_001F32FD(void) { recomp_program_manual_call(0x001f32fdu); }
void sub_001F3332(void) { recomp_program_manual_call(0x001f3332u); }
void sub_001F2D0C(void) { recomp_program_manual_call(0x001f2d0cu); }
void sub_001F3367(void) { recomp_program_manual_call(0x001f3367u); }
void sub_001F35EB(void) { recomp_program_manual_call(0x001f35ebu); }
void sub_001F1CEF(void) { recomp_program_manual_call(0x001f1cefu); }
void sub_001F26C9(void) { recomp_program_manual_call(0x001f26c9u); }
void sub_001F271D(void) { recomp_program_manual_call(0x001f271du); }
void sub_001F2D40(void) { recomp_program_manual_call(0x001f2d40u); }
void sub_001F2D64(void) { recomp_program_manual_call(0x001f2d64u); }
void sub_001F2D99(void) { recomp_program_manual_call(0x001f2d99u); }
void sub_001F2775(void) { recomp_program_manual_call(0x001f2775u); }
void sub_001F2799(void) { recomp_program_manual_call(0x001f2799u); }
void sub_001F27B9(void) { recomp_program_manual_call(0x001f27b9u); }
void sub_001F27D5(void) { recomp_program_manual_call(0x001f27d5u); }
void sub_001F27F5(void) { recomp_program_manual_call(0x001f27f5u); }
void sub_001F2811(void) { recomp_program_manual_call(0x001f2811u); }
void sub_001F2D24(void) { recomp_program_manual_call(0x001f2d24u); }
void sub_001F2701(void) { recomp_program_manual_call(0x001f2701u); }
void sub_001F2739(void) { recomp_program_manual_call(0x001f2739u); }
void sub_001F275D(void) { recomp_program_manual_call(0x001f275du); }
void sub_001F3387(void) { recomp_program_manual_call(0x001f3387u); }

void sub_00193140(void) { recomp_program_manual_call(0x00193140u); }

/* CRI ADXF_GetPtStat (0x00169150) and ADXF_GetStat (0x00168DE0), which the
   game polls without yielding. The ADXM server threads that complete reads
   never run here, so each poll first runs server passes until they stop
   reading, then the generated status body. */
extern void sub_00191470_gen(void);
void sub_00191470(void)
{
    recomp_cri_adxm_server_step();
    sub_00191470_gen();
}

extern void sub_00191110_gen(void);
void sub_00191110(void)
{
    recomp_cri_adxm_server_step();
    sub_00191110_gen();
}

/* 0x00027720 (USA 0x0002A540) builds a camera object's view and
   projection, the object in EAX; the free camera puts its eye, target and
   field of view in first (free_camera_adapter.c). */
extern void sub_000276F0_gen(void);
void sub_000276F0(void) { recomp_free_camera_build(sub_000276F0_gen); }

/* The main menu's Quit Game entry (menu_quit.c): the entry list (0x000CDD50),
   its reader by position (0x000CDD20), the two help line updates
   (0x000CE530, 0x000CBDA0), the carousel draw (0x000CE070), the two sprite
   draws (0x0006DCA0, 0x0006DD10) and the menu's frame (0x000D6420). */
extern void sub_000CDD50_gen(void);
void sub_000CDD50(void) { recomp_menu_quit_build(sub_000CDD50_gen); }
extern void sub_000CDD20_gen(void);
void sub_000CDD20(void) { recomp_menu_quit_entry(sub_000CDD20_gen); }
extern void sub_000CE530_gen(void);
void sub_000CE530(void) { recomp_menu_quit_help(sub_000CE530_gen); }
extern void sub_000CBDA0_gen(void);
void sub_000CBDA0(void) { recomp_menu_quit_help(sub_000CBDA0_gen); }
extern void sub_000CE070_gen(void);
void sub_000CE070(void) { recomp_menu_quit_carousel(sub_000CE070_gen); }
extern void sub_0006DCA0_gen(void);
void sub_0006DCA0(void) { recomp_menu_quit_draw(sub_0006DCA0_gen); }
extern void sub_0006DD10_gen(void);
void sub_0006DD10(void) { recomp_menu_quit_draw(sub_0006DD10_gen); }
extern void sub_000D6420_gen(void);
void sub_000D6420(void) { recomp_menu_quit_update(sub_000D6420_gen); }

/* D3D8 3925 draw, binding and render-state APIs. */
void sub_001DE290(void) { recomp_program_manual_call(0x001de290u); }

/* DOA3 Sofdec planar color conversion (cdecl). */
void sub_001A3DF0(void) { recomp_program_manual_call(0x001a3df0u); }

/* audio: DirectSoundUseFullHRTF, DSOUND 3936. Host stereo has no HRTF table. */
void sub_001F1D05(void) { recomp_program_manual_call(0x001f1d05u); }
void sub_001F1CE5(void) { recomp_program_manual_call(0x001f1ce5u); }
void sub_001F1CEA(void) { recomp_program_manual_call(0x001f1ceau); }
/* DSOUND 4134 IDirectSoundBuffer_SetHeadroom; Japan and Europe only. */
void sub_001F26E5(void) { recomp_program_manual_call(0x001f26e5u); }
/* audio end. */

/* av: CRI ADXM vblank workers. */
void sub_00193060(void) { recomp_program_manual_call(0x00193060u); }
void sub_001930D0(void) { recomp_program_manual_call(0x001930d0u); }
/* av end. */

/* Hand-written game collision passes; geometry helpers remain lifted. */

/* Game: KO afterimage blend setup (0x0006BF00). The original sets the blend
   factors to SRCALPHA and INVSRCALPHA, through D3DDevice_SetRenderState_Simple
   and D3D's state copies, 1,000,000 times. Only the final state is observable
   here, and through the render-state adapter the repeats cost about a second
   per frame. On the Xbox they kept the CPU and GPU busy, which slows the KO,
   so set the state once and wait as long instead. */
void sub_0006BD30(void)
{
    static const uint32_t states[2][3] = {
        {0x00040344u, 0x302u, 0x001eb9a8u},
        {0x00040348u, 0x303u, 0x001eb9acu},
    };
    const uint32_t entry_esp = recomp_runtime.registers.esp;

    for (unsigned i = 0u; i < 2u; ++i) {
        recomp_runtime.registers.ecx = states[i][0];
        recomp_runtime.registers.edx = states[i][1];
        kernel_call_guest(0x001de7f0u, NULL, 0u);
        *recomp_memory_u32(states[i][2]) = states[i][1];
    }
    /* shortcut: 30 ms is estimated (about 15 cycles per pass at 733 MHz, and
       16 MB of push buffer for the GPU); calibrate against hardware footage.
       KeDelayExecutionThread(KernelMode, FALSE, relative 100 ns units). */
    recomp_runtime.registers.esp = entry_esp - 8u;
    *recomp_memory_u64(recomp_runtime.registers.esp) = (uint64_t)-300000;
    kernel_call_guest(0x80000063u,
        (const uint32_t[]){0u, 0u, recomp_runtime.registers.esp}, 3u);
    recomp_runtime.registers.esp = entry_esp;
    recomp_runtime.registers.esp += 4u; /* ret */
}

/* Hand-written DOA3 auto-save, bracketed by the save journal (save_adapter.c). */

/* Europe entry point -> the address its adapter is keyed by. Only these reach
   the adapters: other USA keys can be unrelated Europe functions. */
static const uint32_t jp_manual[][2] = {
    {0x0018d2a4u, 0x00164f50u},
    {0x0018d330u, 0x00164fdcu},
    {0x0018d343u, 0x00164fefu},
    {0x0018d382u, 0x0016502eu},
    {0x00193060u, 0x0016a570u},
    {0x001930d0u, 0x0016a5e0u},
    {0x00193140u, 0x0016a650u},
    {0x001a3df0u, 0x001779d0u},
    {0x001dcca0u, 0x001b0e00u},
    {0x001dcfa0u, 0x001b1130u},
    {0x001ddfb0u, 0x001b1cc0u},
    {0x001de1c0u, 0x001b1ea0u},
    {0x001de290u, 0x001b1f30u},
    {0x001de7f0u, 0x001b2390u},
    {0x001de9c0u, 0x001b2470u},
    {0x001dea90u, 0x001b2520u},
    {0x001deb40u, 0x001b25d0u},
    {0x001deb80u, 0x001b2600u},
    {0x001dedb0u, 0x001b2800u},
    {0x001df580u, 0x001b3040u},
    {0x001df620u, 0x001b30b0u},
    {0x001df6b0u, 0x001b3130u},
    {0x001df890u, 0x001b32f0u},
    {0x001df950u, 0x001b3390u},
    {0x001dfdd0u, 0x001b3760u},
    {0x001dff30u, 0x001b38a0u},
    {0x001dffd0u, 0x001b3940u},
    {0x001e0890u, 0x001b4230u},
    {0x001e0c50u, 0x001b45f0u},
    {0x001e1e10u, 0x001e14d0u},
    {0x001e30d0u, 0x001b8890u},
    {0x001e3100u, 0x001b88c0u},
    {0x001e5098u, 0x001ba7d8u},
    {0x001e5bc3u, 0x001bb1f3u},
    {0x001e5c26u, 0x001bb256u},
    {0x001e5f63u, 0x001bb595u},
    {0x001e6032u, 0x001bb66au},
    {0x001e612du, 0x001bb770u},
    {0x001e628fu, 0x001bb8a8u},
    {0x001e647du, 0x001bba96u},
    {0x001f1ce5u, 0x001c6b72u},
    {0x001f1ceau, 0x001c6b77u},
    {0x001f1cefu, 0x001c6b7cu},
    {0x001f1d05u, 0x001c6b92u},
    {0x001f2686u, 0x001c7392u},
    {0x001f26a2u, 0x001c73aeu},
    {0x001f26c9u, 0x001c7477u},
    {0x001f26e5u, 0x001f26e5u},
    {0x001f2701u, 0x001c7b17u},
    {0x001f271du, 0x001c7493u},
    {0x001f2739u, 0x001c7b33u},
    {0x001f275du, 0x001c7b57u},
    {0x001f2775u, 0x001c7541u},
    {0x001f2799u, 0x001c7565u},
    {0x001f27b9u, 0x001c7585u},
    {0x001f27d5u, 0x001c75a1u},
    {0x001f27f5u, 0x001c75c1u},
    {0x001f2811u, 0x001c75ddu},
    {0x001f2841u, 0x001c760du},
    {0x001f2d0cu, 0x001c743fu},
    {0x001f2d24u, 0x001c7afbu},
    {0x001f2d40u, 0x001c74b3u},
    {0x001f2d64u, 0x001c74d7u},
    {0x001f2d99u, 0x001c750cu},
    {0x001f32fdu, 0x001c73d5u},
    {0x001f3332u, 0x001c740au},
    {0x001f3367u, 0x001c7457u},
    {0x001f3387u, 0x001c7b6fu},
    {0x001f35ebu, 0x001c7ea9u},
    {0x001f37a5u, 0x001c7fd9u},
    {0x002114acu, 0x001e5d4cu},
    {0x00211eb8u, 0x001e6953u},
    {0x00211ebdu, 0x001e6958u},
    {0x00211edfu, 0x001e697au},
    {0x00212163u, 0x001e6e3au},
    {0x002121d8u, 0x001e6eafu},
    {0x002121e4u, 0x001e6ebbu},
    {0x002123d6u, 0x001e70adu},
    {0x00212447u, 0x001e711eu},
};

uint32_t recomp_doa3_jp_usa_address(uint32_t guest_address)
{
    size_t low = 0u, high = sizeof jp_manual / sizeof jp_manual[0];

    while (low < high) {
        size_t mid = (low + high) / 2u;
        if (jp_manual[mid][0] == guest_address) return jp_manual[mid][1];
        if (jp_manual[mid][0] < guest_address) low = mid + 1u; else high = mid;
    }
    return 0u;
}
