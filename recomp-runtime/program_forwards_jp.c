/* Japan 3.1 (XDK 4134) forwards, generated from program_forwards.c with
   the USA-to-Japan Version Tracking map. Each JP entry point passes the
   USA address of the function it replaces, so USA-keyed adapters bind.
   shortcut: a committed copy for the JP boot test; generate it in the
   build if JP becomes a real target. */
/* Library entry points rebound to hand-written adapters.

   The lifter generates no body for an address listed in
   tools/doa3/manual_functions.json and emits every call to it as indirect
   dispatch, which tries recomp_lookup_manual() first. The dispatch table and
   recomp_funcs.h still name sub_X, so define it here as one
   recomp_program_manual_call() call, e.g.
     void sub_001DF010(void) { recomp_program_manual_call(0x001b3390u); }
   It runs only when no adapter is bound, and then stops naming the address. */
#include "program_manual.h"
#include "stop_report.h"
#include "cri_adxm_adapter.h"

#include <inttypes.h>

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
void sub_0018C934(void) { recomp_program_manual_call(0x00164f50u); }
void sub_0018C9C0(void) { recomp_program_manual_call(0x00164fdcu); }
void sub_0018C9D3(void) { recomp_program_manual_call(0x00164fefu); }
void sub_0018CA12(void) { recomp_program_manual_call(0x0016502eu); }
/* FIBER end. */

/* D3DDevice_SetGammaRamp, D3DDevice_Clear, D3DDevice_Present (XDK 3925).
   Their adapters are in d3d_frame_adapter.c; see docs/doa3-d3d-symbols.md. */
void sub_001DC360(void) { recomp_program_manual_call(0x001b0e00u); }
void sub_001DF010(void) { recomp_program_manual_call(0x001b3390u); }
void sub_001DC660(void) { recomp_program_manual_call(0x001b1130u); }
void sub_001E14D0(void) { recomp_program_manual_call(0x001e14d0u); }

/* XInitDevices and its jmp thunk, XGetDevices, XGetDeviceChanges, XInputOpen,
   XInputClose, XInputGetCapabilities, XInputGetState, XInputSetState
   (XAPILIB 3911, XPP section). Adapters are in input_adapter.c. */
void sub_00210B6C(void) { recomp_program_manual_call(0x001e5d4cu); }
void sub_00211578(void) { recomp_program_manual_call(0x001e6953u); }
void sub_0021157D(void) { recomp_program_manual_call(0x001e6958u); }
void sub_0021159F(void) { recomp_program_manual_call(0x001e697au); }
void sub_00211823(void) { recomp_program_manual_call(0x001e6e3au); }
void sub_00211898(void) { recomp_program_manual_call(0x001e6eafu); }
void sub_002118A4(void) { recomp_program_manual_call(0x001e6ebbu); }
void sub_00211A96(void) { recomp_program_manual_call(0x001e70adu); }
void sub_00211B07(void) { recomp_program_manual_call(0x001e711eu); }

/* CDevice::KickOff and the miniport GPU layer under CDevice::Init (D3D8 3925).
   Adapters are in d3d_miniport_adapter.c. */
void sub_001E27C0(void) { recomp_program_manual_call(0x001b88c0u); }
void sub_001E57ED(void) { recomp_program_manual_call(0x001bb770u); }
void sub_001E5283(void) { recomp_program_manual_call(0x001bb1f3u); }
void sub_001E52E6(void) { recomp_program_manual_call(0x001bb256u); }
void sub_001E5623(void) { recomp_program_manual_call(0x001bb595u); }
void sub_001E56F2(void) { recomp_program_manual_call(0x001bb66au); }
void sub_001E4758(void) { recomp_program_manual_call(0x001ba7d8u); }
void sub_001E594F(void) { recomp_program_manual_call(0x001bb8a8u); }
void sub_001E5B3D(void) { recomp_program_manual_call(0x001bba96u); }
void sub_001E2790(void) { recomp_program_manual_call(0x001b8890u); }

/* DirectSound 3936 wrappers game code calls (dsound_api_adapter.c). */
void sub_001F2E65(void) { recomp_program_manual_call(0x001c7fd9u); }
void sub_001F1F01(void) { recomp_program_manual_call(0x001c760du); }
void sub_001F1D46(void) { recomp_program_manual_call(0x001c7392u); }
void sub_001F1D62(void) { recomp_program_manual_call(0x001c73aeu); }
void sub_001F29BD(void) { recomp_program_manual_call(0x001c73d5u); }
void sub_001F29F2(void) { recomp_program_manual_call(0x001c740au); }
void sub_001F23CC(void) { recomp_program_manual_call(0x001c743fu); }
void sub_001F2A27(void) { recomp_program_manual_call(0x001c7457u); }
void sub_001F2CAB(void) { recomp_program_manual_call(0x001c7ea9u); }
void sub_001F13AF(void) { recomp_program_manual_call(0x001c6b7cu); }
void sub_001F1D89(void) { recomp_program_manual_call(0x001c7477u); }
void sub_001F1DDD(void) { recomp_program_manual_call(0x001c7493u); }
void sub_001F2400(void) { recomp_program_manual_call(0x001c74b3u); }
void sub_001F2424(void) { recomp_program_manual_call(0x001c74d7u); }
void sub_001F2459(void) { recomp_program_manual_call(0x001c750cu); }
void sub_001F1E35(void) { recomp_program_manual_call(0x001c7541u); }
void sub_001F1E59(void) { recomp_program_manual_call(0x001c7565u); }
void sub_001F1E79(void) { recomp_program_manual_call(0x001c7585u); }
void sub_001F1E95(void) { recomp_program_manual_call(0x001c75a1u); }
void sub_001F1EB5(void) { recomp_program_manual_call(0x001c75c1u); }
void sub_001F1ED1(void) { recomp_program_manual_call(0x001c75ddu); }
void sub_001F23E4(void) { recomp_program_manual_call(0x001c7afbu); }
void sub_001F1DC1(void) { recomp_program_manual_call(0x001c7b17u); }
void sub_001F1DF9(void) { recomp_program_manual_call(0x001c7b33u); }
void sub_001F1E1D(void) { recomp_program_manual_call(0x001c7b57u); }
void sub_001F2A47(void) { recomp_program_manual_call(0x001c7b6fu); }

void sub_001927D0(void) { recomp_program_manual_call(0x0016a650u); }

/* CRI ADXF_GetPtStat (0x00169150) and ADXF_GetStat (0x00168DE0), which the
   game polls without yielding. The ADXM server threads that complete reads
   never run here, so each poll first runs one server pass, then the generated
   status body. */
extern void sub_00190B00_gen(void);
void sub_00190B00(void)
{
    recomp_cri_adxm_server_step();
    sub_00190B00_gen();
}

extern void sub_001907A0_gen(void);
void sub_001907A0(void)
{
    recomp_cri_adxm_server_step();
    sub_001907A0_gen();
}

/* D3D8 3925 draw, binding and render-state APIs. */
void sub_001DF490(void) { recomp_program_manual_call(0x001b3760u); }
void sub_001DF5F0(void) { recomp_program_manual_call(0x001b38a0u); }
void sub_001DF690(void) { recomp_program_manual_call(0x001b3940u); }
void sub_001DD670(void) { recomp_program_manual_call(0x001b1cc0u); }
void sub_001E0310(void) { recomp_program_manual_call(0x001b45f0u); }
void sub_001DFF50(void) { recomp_program_manual_call(0x001b4230u); }
void sub_001DD880(void) { recomp_program_manual_call(0x001b1ea0u); }
void sub_001DDEB0(void) { recomp_program_manual_call(0x001b2390u); }
void sub_001DE080(void) { recomp_program_manual_call(0x001b2470u); }
void sub_001DE150(void) { recomp_program_manual_call(0x001b2520u); }
void sub_001DE200(void) { recomp_program_manual_call(0x001b25d0u); }
void sub_001DE240(void) { recomp_program_manual_call(0x001b2600u); }
void sub_001DE470(void) { recomp_program_manual_call(0x001b2800u); }
void sub_001DEC40(void) { recomp_program_manual_call(0x001b3040u); }
void sub_001DECE0(void) { recomp_program_manual_call(0x001b30b0u); }
void sub_001DED70(void) { recomp_program_manual_call(0x001b3130u); }
void sub_001DEF50(void) { recomp_program_manual_call(0x001b32f0u); }
void sub_001DD950(void) { recomp_program_manual_call(0x001b1f30u); }

/* DOA3 Sofdec planar color conversion (cdecl). */
void sub_001A3480(void) { recomp_program_manual_call(0x001779d0u); }

/* audio: DirectSoundUseFullHRTF, DSOUND 3936. Host stereo has no HRTF table. */
void sub_001F13C5(void) { recomp_program_manual_call(0x001c6b92u); }
void sub_001F13A5(void) { recomp_program_manual_call(0x001c6b72u); }
void sub_001F13AA(void) { recomp_program_manual_call(0x001c6b77u); }
/* audio end. */

/* av: CRI ADXM vblank workers. */
void sub_001926F0(void) { recomp_program_manual_call(0x0016a570u); }
void sub_00192760(void) { recomp_program_manual_call(0x0016a5e0u); }
/* av end. */

/* Hand-written game collision passes; geometry helpers remain lifted. */
void sub_000A8E20(void) { recomp_program_manual_call(0x000a1380u); }
void sub_000AC950(void) { recomp_program_manual_call(0x000a9ac0u); }
void sub_00093210(void) { recomp_program_manual_call(0x0008d180u); }
void sub_000A8B10(void) { recomp_program_manual_call(0x000a40e0u); }

/* Hand-written DOA3 auto-save, bracketed by the save journal (save_adapter.c). */
void sub_00020C80(void) { recomp_program_manual_call(0x00021930u); }
