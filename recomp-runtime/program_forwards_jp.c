/* Japan 3.1 (XDK 4134), generated from program_forwards.c with the
   USA-to-Japan Version Tracking map; see docs/doa3-jp-test.md.
   shortcut: a committed copy for the JP boot test; generate it in the
   build if JP becomes a real target. */
/* Library entry points rebound to hand-written adapters.

   The lifter generates no body for an address listed in
   tools/doa3/manual_functions.json and emits every call to it as indirect
   dispatch, which tries recomp_lookup_manual() first. The dispatch table and
   recomp_funcs.h still name sub_X, so define it here as one
   recomp_program_manual_call() call, e.g.
     void sub_001DF010(void) { recomp_program_manual_call(0x001df010u); }
   It runs only when no adapter is bound, and then stops naming the address. */
#include "program_manual.h"
#include "stop_report.h"
#include "cri_adxm_adapter.h"
#include "kernel_abi.h"
#include "free_camera_adapter.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
void sub_0018C934(void) { recomp_program_manual_call(0x0018c934u); }
void sub_0018C9C0(void) { recomp_program_manual_call(0x0018c9c0u); }
void sub_0018C9D3(void) { recomp_program_manual_call(0x0018c9d3u); }
void sub_0018CA12(void) { recomp_program_manual_call(0x0018ca12u); }
/* FIBER end. */

/* D3DDevice_SetGammaRamp, D3DDevice_Clear, D3DDevice_Present (XDK 3925).
   Their adapters are in d3d_frame_adapter.c; see docs/doa3-d3d-symbols.md. */
void sub_001DC360(void) { recomp_program_manual_call(0x001dc360u); }
void sub_001DF010(void) { recomp_program_manual_call(0x001df010u); }
void sub_001DC660(void) { recomp_program_manual_call(0x001dc660u); }
void sub_001E14D0(void) { recomp_program_manual_call(0x001e14d0u); }

/* D3D8 4134 draw, texture, vertex shader and render-state setters; adapters
   in d3d_draw_adapter.c, d3d_texture_adapter.c, d3d_vertex_shader_adapter.c
   and d3d_render_state_adapter.c. */
void sub_001DD670(void) { recomp_program_manual_call(0x001dd670u); }
void sub_001DD880(void) { recomp_program_manual_call(0x001dd880u); }
void sub_001DDEB0(void) { recomp_program_manual_call(0x001ddeb0u); }
void sub_001DE080(void) { recomp_program_manual_call(0x001de080u); }
void sub_001DE150(void) { recomp_program_manual_call(0x001de150u); }
void sub_001DE200(void) { recomp_program_manual_call(0x001de200u); }
void sub_001DE240(void) { recomp_program_manual_call(0x001de240u); }
void sub_001DE470(void) { recomp_program_manual_call(0x001de470u); }
void sub_001DEC40(void) { recomp_program_manual_call(0x001dec40u); }
void sub_001DECE0(void) { recomp_program_manual_call(0x001dece0u); }
void sub_001DED70(void) { recomp_program_manual_call(0x001ded70u); }
void sub_001DEF50(void) { recomp_program_manual_call(0x001def50u); }
void sub_001DF490(void) { recomp_program_manual_call(0x001df490u); }
void sub_001DF5F0(void) { recomp_program_manual_call(0x001df5f0u); }
void sub_001DF690(void) { recomp_program_manual_call(0x001df690u); }
void sub_001DFF50(void) { recomp_program_manual_call(0x001dff50u); }
void sub_001E0310(void) { recomp_program_manual_call(0x001e0310u); }

/* XInitDevices and its jmp thunk, XGetDevices, XGetDeviceChanges, XInputOpen,
   XInputClose, XInputGetCapabilities, XInputGetState, XInputSetState
   (XAPILIB 3911, XPP section). Adapters are in input_adapter.c. */
void sub_00210B6C(void) { recomp_program_manual_call(0x00210b6cu); }
void sub_00211578(void) { recomp_program_manual_call(0x00211578u); }
void sub_0021157D(void) { recomp_program_manual_call(0x0021157du); }
void sub_0021159F(void) { recomp_program_manual_call(0x0021159fu); }
void sub_00211823(void) { recomp_program_manual_call(0x00211823u); }
void sub_00211898(void) { recomp_program_manual_call(0x00211898u); }
void sub_002118A4(void) { recomp_program_manual_call(0x002118a4u); }
void sub_00211A96(void) { recomp_program_manual_call(0x00211a96u); }
void sub_00211B07(void) { recomp_program_manual_call(0x00211b07u); }

/* CDevice::KickOff and the miniport GPU layer under CDevice::Init (D3D8 3925).
   Adapters are in d3d_miniport_adapter.c. */
void sub_001E27C0(void) { recomp_program_manual_call(0x001e27c0u); }
void sub_001E57ED(void) { recomp_program_manual_call(0x001e57edu); }
void sub_001E5283(void) { recomp_program_manual_call(0x001e5283u); }
void sub_001E52E6(void) { recomp_program_manual_call(0x001e52e6u); }
void sub_001E5623(void) { recomp_program_manual_call(0x001e5623u); }
void sub_001E56F2(void) { recomp_program_manual_call(0x001e56f2u); }
void sub_001E4758(void) { recomp_program_manual_call(0x001e4758u); }
void sub_001E594F(void) { recomp_program_manual_call(0x001e594fu); }
void sub_001E5B3D(void) { recomp_program_manual_call(0x001e5b3du); }
void sub_001E2790(void) { recomp_program_manual_call(0x001e2790u); }

/* DirectSound 3936 wrappers game code calls (dsound_api_adapter.c). */
void sub_001F2E65(void) { recomp_program_manual_call(0x001f2e65u); }
void sub_001F1F01(void) { recomp_program_manual_call(0x001f1f01u); }
void sub_001F1D46(void) { recomp_program_manual_call(0x001f1d46u); }
void sub_001F1D62(void) { recomp_program_manual_call(0x001f1d62u); }
void sub_001F29BD(void) { recomp_program_manual_call(0x001f29bdu); }
void sub_001F29F2(void) { recomp_program_manual_call(0x001f29f2u); }
void sub_001F23CC(void) { recomp_program_manual_call(0x001f23ccu); }
void sub_001F2A27(void) { recomp_program_manual_call(0x001f2a27u); }
void sub_001F2CAB(void) { recomp_program_manual_call(0x001f2cabu); }
void sub_001F13AF(void) { recomp_program_manual_call(0x001f13afu); }
void sub_001F1D89(void) { recomp_program_manual_call(0x001f1d89u); }
void sub_001F1DDD(void) { recomp_program_manual_call(0x001f1dddu); }
void sub_001F2400(void) { recomp_program_manual_call(0x001f2400u); }
void sub_001F2424(void) { recomp_program_manual_call(0x001f2424u); }
void sub_001F2459(void) { recomp_program_manual_call(0x001f2459u); }
void sub_001F1E35(void) { recomp_program_manual_call(0x001f1e35u); }
void sub_001F1E59(void) { recomp_program_manual_call(0x001f1e59u); }
void sub_001F1E79(void) { recomp_program_manual_call(0x001f1e79u); }
void sub_001F1E95(void) { recomp_program_manual_call(0x001f1e95u); }
void sub_001F1EB5(void) { recomp_program_manual_call(0x001f1eb5u); }
void sub_001F1ED1(void) { recomp_program_manual_call(0x001f1ed1u); }
void sub_001F23E4(void) { recomp_program_manual_call(0x001f23e4u); }
void sub_001F1DC1(void) { recomp_program_manual_call(0x001f1dc1u); }
void sub_001F1DF9(void) { recomp_program_manual_call(0x001f1df9u); }
void sub_001F1E1D(void) { recomp_program_manual_call(0x001f1e1du); }
void sub_001F2A47(void) { recomp_program_manual_call(0x001f2a47u); }

void sub_001927D0(void) { recomp_program_manual_call(0x001927d0u); }

/* CRI ADXF_GetPtStat (0x00169150) and ADXF_GetStat (0x00168DE0), which the
   game polls without yielding. The ADXM server threads that complete reads
   never run here, so each poll first runs server passes until they stop
   reading, then the generated status body. */
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

/* 0x00027720 (USA 0x0002A540) builds a camera object's view and
   projection, the object in EAX; the free camera puts its eye, target and
   field of view in first (free_camera_adapter.c). */
extern void sub_00027720_gen(void);
void sub_00027720(void) { recomp_free_camera_build(sub_00027720_gen); }

/* D3D8 3925 draw, binding and render-state APIs. */
void sub_001DD950(void) { recomp_program_manual_call(0x001dd950u); }

/* DOA3 Sofdec planar color conversion (cdecl). */
void sub_001A3480(void) { recomp_program_manual_call(0x001a3480u); }

/* audio: DirectSoundUseFullHRTF, DSOUND 3936. Host stereo has no HRTF table. */
void sub_001F13C5(void) { recomp_program_manual_call(0x001f13c5u); }
void sub_001F13A5(void) { recomp_program_manual_call(0x001f13a5u); }
void sub_001F13AA(void) { recomp_program_manual_call(0x001f13aau); }
/* DSOUND 4134 IDirectSoundBuffer_SetHeadroom; Japan only. */
void sub_001F1DA5(void) { recomp_program_manual_call(0x001f1da5u); }
/* audio end. */

/* av: CRI ADXM vblank workers. */
void sub_001926F0(void) { recomp_program_manual_call(0x001926f0u); }
void sub_00192760(void) { recomp_program_manual_call(0x00192760u); }
/* av end. */

/* Hand-written game collision passes; geometry helpers remain lifted. */

/* Game: KO afterimage blend setup (0x0006BF00). The original sets the blend
   factors to SRCALPHA and INVSRCALPHA, through D3DDevice_SetRenderState_Simple
   and D3D's state copies, 1,000,000 times. Only the final state is observable
   here, and through the render-state adapter the repeats cost about a second
   per frame. On the Xbox they kept the CPU and GPU busy, which slows the KO,
   so set the state once and wait as long instead. */
void sub_0006BF00(void)
{
    static const uint32_t states[2][3] = {
        {0x00040344u, 0x302u, 0x001eb068u},
        {0x00040348u, 0x303u, 0x001eb06cu},
    };
    const uint32_t entry_esp = recomp_runtime.registers.esp;

    for (unsigned i = 0u; i < 2u; ++i) {
        recomp_runtime.registers.ecx = states[i][0];
        recomp_runtime.registers.edx = states[i][1];
        kernel_call_guest(0x001ddeb0u, NULL, 0u);
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

/* Extended roster. The lifter moves the per-character tables below into a
   runtime-owned guest region above RAM (tools/doa3-jp/data_relocations.json
   lists the same ranges), so every lifted reader uses the larger copy. The
   copies start as the XBE's tables; roster.txt in the disc folder adds
   characters, select cells and sprites. Without it the game is stock.

   roster.txt, one entry per line, '#' comments, numbers in C syntax:
     sprite ID U0 V0 U1 V1 TEX    sprite table row (pixel rect, texture id)
     char ID key=value...         an added character, ID 25..49:
       like=ID                    copy every row of a stock character (put it
                                  first; later keys override those rows)
       package= bank= bank1= bank2= bank3= select_bank=   loadfile ids
                                  (bank1..3 default to bank)
       compact=ID                 profile it shares: voice set, unlocks, records
       voice=ID                   stock fighter whose 0x404b1c sound bank it
                                  loads (default like; see 0x00085e40)
       clip= param=               select-screen preview clip and parameter
       body=FILE                  0x110-byte body block beside roster.txt
       costume=M1:M0[:C2:C3:C4]   one per costume, in picker order; missing
                                  columns come from like's first costume
     grid CHAR KIND BIG SMALL NAME STYLE   a select cell (sprite ids) */
#define ROSTER_BASE 0x04000000u
#define ROSTER_SIZE 0x00020000u
#define R(offset) (ROSTER_BASE + (offset))
#define GRID_RECORDS R(0x0000u)    /* 10-byte select cell records */
#define GRID_SLOTS R(0x1000u)      /* cell -> record */
#define GRID_OFFSETS R(0x1100u)    /* u16 cell x offset */
#define CHAR_CELL R(0x1400u)       /* char -> first cell */
#define CHAR_COMPACT R(0x1500u)    /* char -> profile */
#define PICK_COUNT R(0x1600u)      /* costume picker: entries */
#define PICK_CAP R(0x1700u)        /* costume picker: costumes */
#define PICK_LIST R(0x1800u)       /* costume picker: -> costume ids */
#define PICK_OUTFIT R(0x1900u)     /* costume picker: -> variants per costume */
#define PREVIEW_CLIP R(0x1A00u)    /* u16 select-screen clip */
#define PREVIEW_PARAM R(0x1B00u)   /* u32 */
#define BODY R(0x1C00u)            /* -> 0x110-byte body block */
#define VARIANT_V0 R(0x2000u)      /* {u16 package, u16 bank} per variant */
#define VARIANT_V1 R(0x2100u)
#define VARIANT_V4 R(0x2200u)      /* select-screen bank */
#define VARIANT_V2 R(0x2300u)
#define VARIANT_V3 R(0x2400u)
#define EYES R(0x2800u)            /* 16 B rows read by fighter_eyes_aim 0x957e0 */
#define COSTUMES R(0x3000u)        /* 5 x u16 per (costume * 25 + char) */
#define SPRITES R(0x8000u)         /* 12-byte rows */
#define BODY_BLOCKS R(0x10000u)
#define PICK_BYTES R(0x15000u)     /* added characters' picker lists */
/* Per-character tables found by their readers' index (the char byte of a
   player record, 0x0059cf59 + 0x68 * player); purpose not decoded yet. */
#define CHAR_2A09D8 R(0x16000u)  /* 0xbbda0 */
#define CHAR_2CC4E8 R(0x16100u)  /* 0xb3950 */
#define CHAR_2CC9C0 R(0x16200u)  /* 0xb3fb0 */
#define CHAR_2CCCC0 R(0x16300u)  /* 0xb3fb0 */
#define CHAR_2CCF80 R(0x16400u)  /* 0xb3fb0 */
#define CHAR_2CD318 R(0x16500u)  /* 0xb4120 */
#define CHAR_2CD720 R(0x16600u)  /* 0xb3e50 */
#define CHAR_2CDD30 R(0x16700u)  /* 0xaec50 */
#define CHAR_2D1DC8 R(0x16800u)  /* 0xb1f60 */
#define CHAR_2D2CE4 R(0x16900u)  /* 0xb0640 0xb37c0 0x14d50 */
#define CHAR_2D3008 R(0x16A00u)  /* 0xafcf0 */
#define CHAR_2D303C R(0x16B00u)  /* 0xafcf0 */
#define CHAR_2D3178 R(0x16C00u)  /* 0xb2a90 */
#define CHAR_2D9530 R(0x16D00u)  /* 0xa31a0 */
#define CHAR_2DA06C R(0x16E00u)  /* 0xa3690 */
#define CHAR_2DC680 R(0x16F00u)  /* 0x99a40 */
#define CHAR_2DC7A8 R(0x17000u)  /* 0x99b30 */
#define CHAR_2DCE80 R(0x17100u)  /* 0x8f290 */
#define CHAR_2DCECC R(0x17200u)  /* 0x8cff0 */
#define CHAR_3034E0 R(0x17300u)  /* 0x7c2a0 0x7dcb0 */
#define CHAR_30AFD8 R(0x17400u)  /* 0x58fc0 */
#define CHAR_30C7E8 R(0x17500u)  /* 0x59690 */
#define CHAR_3EC608 R(0x17600u)  /* 0x2a810 0x31360 */
#define CHAR_3E3C48 R(0x17700u)  /* 0x37310 */
#define CHAR_3E3D48 R(0x17800u)  /* 0x37310 */
#define CHAR_26C728 R(0x17900u)  /* 0x1832d0 0x184000 0x1849f0 */
#define CHAR_259D54 R(0x17A00u)  /* 0x186e40 */
#define CHAR_2204B8 R(0x17B00u)  /* 0x120f0 */
#define CHAR_2CBE18 R(0x17C00u)  /* 0xa1850 */
#define CHAR_2CBE80 R(0x17D00u)  /* 0xba6c0 */
#define CHAR_2D9FA0 R(0x17E00u)  /* 0xa0f70 0xa0ca0 0x14d50 */
#define CHAR_2DA008 R(0x17F00u)  /* 0xa0f70 */
#define CHAR_2CD784 R(0x18000u)  /* 0xb3e50 */
#define CHAR_2D2900 R(0x18100u)  /* 0xb1680 0xb18d0 0x8cff0 */
#define CHAR_22042C R(0x18300u)  /* 0x193b0 0x19400 */
#define CHAR_259CF0 R(0x18400u)  /* 0x181910 0x182080 0x186e40 */
#define CPU_SCRIPT R(0x18500u)   /* CPU script pointer, read by 0x19790 */

#define STOCK_CHARS 25u
#define ROSTER_CHARS 50u           /* see costume_row */
#define CHAR_PROXY 23u             /* CHIN: no select cell, no picker entries */
#define GRID_RECORD_MAX 0xFDu      /* char ids 0xFE/0xFF are special */
#define GRID_STOCK_RECORDS 18u
#define GRID_COUNT 0x0029CB14u
#define EIN_UNLOCKED 0x005A0B47u
#define CHAR_EIN 3u
#define CHAR_RANDOM 0xFFu
#define SPRITE_MAX ((BODY_BLOCKS - SPRITES) / 12u)

static const uint32_t roster_relocations[][3] = {
    {0x0029CA60u, 0x00B4u, GRID_RECORDS},
    {0x009B76B0u, 0x0012u, GRID_SLOTS},
    {0x009B7880u, 0x0024u, GRID_OFFSETS},
    {0x009B7688u, 0x0019u, CHAR_CELL},
    {0x002FF868u, 0x0019u, CHAR_COMPACT},
    {0x0029C660u, 0x0019u, PICK_COUNT},
    {0x0029C70Cu, 0x0019u, PICK_CAP},
    {0x0029C728u, 0x0064u, PICK_LIST},
    {0x0029C790u, 0x0064u, PICK_OUTFIT},
    {0x0029BE08u, 0x0032u, PREVIEW_CLIP},
    {0x0029C878u, 0x0064u, PREVIEW_PARAM},
    {0x002DC578u, 0x0064u, BODY},
    {0x002DAC30u, 0x0190u, EYES},
    {0x00301438u, 0x0068u, VARIANT_V0},
    {0x003014A0u, 0x0068u, VARIANT_V1},
    {0x00301508u, 0x0068u, VARIANT_V4},
    {0x00301570u, 0x0068u, VARIANT_V2},
    {0x003015D8u, 0x0068u, VARIANT_V3},
    {0x00301640u, 0x109Au, COSTUMES},
    {0x003049B8u, 0x48FCu, SPRITES},
    {0x002A09D8u, 0x0064u, CHAR_2A09D8},
    {0x002CC4E8u, 0x0064u, CHAR_2CC4E8},
    {0x002CC9C0u, 0x0064u, CHAR_2CC9C0},
    {0x002CCCC0u, 0x0064u, CHAR_2CCCC0},
    {0x002CCF80u, 0x0064u, CHAR_2CCF80},
    {0x002CD318u, 0x0064u, CHAR_2CD318},
    {0x002CD720u, 0x0064u, CHAR_2CD720},
    {0x002CDD30u, 0x0064u, CHAR_2CDD30},
    {0x002D1DC8u, 0x0064u, CHAR_2D1DC8},
    {0x002D2CE4u, 0x0019u, CHAR_2D2CE4},
    {0x002D3008u, 0x0032u, CHAR_2D3008},
    {0x002D303Cu, 0x0032u, CHAR_2D303C},
    {0x002D3178u, 0x0064u, CHAR_2D3178},
    {0x002D9530u, 0x0064u, CHAR_2D9530},
    {0x002DA06Cu, 0x0019u, CHAR_2DA06C},
    {0x002DC680u, 0x0064u, CHAR_2DC680},
    {0x002DC7A8u, 0x0064u, CHAR_2DC7A8},
    {0x002DCE80u, 0x0019u, CHAR_2DCE80},
    {0x002DCECCu, 0x0019u, CHAR_2DCECC},
    {0x003034E0u, 0x0019u, CHAR_3034E0},
    {0x0030AFD8u, 0x0064u, CHAR_30AFD8},
    {0x0030C7E8u, 0x0064u, CHAR_30C7E8},
    {0x003EC608u, 0x0064u, CHAR_3EC608},
    {0x003E3C48u, 0x0064u, CHAR_3E3C48},
    {0x003E3D48u, 0x0064u, CHAR_3E3D48},
    {0x0026C728u, 0x0064u, CHAR_26C728},
    {0x00259D54u, 0x0019u, CHAR_259D54},
    {0x002204B8u, 0x0064u, CHAR_2204B8},
    {0x002CBE18u, 0x0064u, CHAR_2CBE18},
    {0x002CBE80u, 0x0064u, CHAR_2CBE80},
    {0x002D9FA0u, 0x0064u, CHAR_2D9FA0},
    {0x002DA008u, 0x0064u, CHAR_2DA008},
    {0x002CD784u, 0x0032u, CHAR_2CD784},
    {0x002D2900u, 0x00C8u, CHAR_2D2900},
    {0x0022042Cu, 0x0032u, CHAR_22042C},
    {0x00259CF0u, 0x0064u, CHAR_259CF0},
    {0x003F5E00u, 0x0018u, CPU_SCRIPT},   /* see recomp_doa3_roster_map */
};
/* Per-character rows: table, bytes per character. */
static const uint32_t roster_char_rows[][2] = {
    {CHAR_COMPACT, 1u}, {PICK_COUNT, 1u}, {PICK_CAP, 1u}, {PICK_LIST, 4u},
    {PICK_OUTFIT, 4u}, {PREVIEW_CLIP, 2u}, {PREVIEW_PARAM, 4u}, {BODY, 4u},
    {EYES, 16u}, {VARIANT_V0, 4u}, {VARIANT_V1, 4u}, {VARIANT_V4, 4u}, {VARIANT_V2, 4u},
    {VARIANT_V3, 4u},
    {CHAR_2A09D8, 4u}, {CHAR_2CC4E8, 4u}, {CHAR_2CC9C0, 4u}, {CHAR_2CCCC0, 4u},
    {CHAR_2CCF80, 4u}, {CHAR_2CD318, 4u}, {CHAR_2CD720, 4u}, {CHAR_2CDD30, 4u},
    {CHAR_2D1DC8, 4u}, {CHAR_2D2CE4, 1u}, {CHAR_2D3008, 2u}, {CHAR_2D303C, 2u},
    {CHAR_2D3178, 4u}, {CHAR_2D9530, 4u}, {CHAR_2DA06C, 1u}, {CHAR_2DC680, 4u},
    {CHAR_2DC7A8, 4u}, {CHAR_2DCE80, 1u}, {CHAR_2DCECC, 1u}, {CHAR_3034E0, 1u},
    {CHAR_30AFD8, 4u}, {CHAR_30C7E8, 4u}, {CHAR_3EC608, 4u}, {CHAR_3E3C48, 4u},
    {CHAR_3E3D48, 4u}, {CHAR_26C728, 4u}, {CHAR_259D54, 1u},
    {CHAR_2204B8, 4u}, {CHAR_2CBE18, 4u}, {CHAR_2CBE80, 4u}, {CHAR_2D9FA0, 4u},
    {CHAR_2DA008, 4u}, {CHAR_2CD784, 2u}, {CHAR_2D2900, 8u},
    {CHAR_22042C, 2u}, {CHAR_259CF0, 4u}, {CPU_SCRIPT, 4u},
};
static uint8_t roster_memory[ROSTER_SIZE];
static unsigned grid_records = GRID_STOCK_RECORDS;
static uint8_t roster_voice[ROSTER_CHARS];  /* added character -> voice= */

static uint8_t *roster_at(uint32_t guest)
{
    return roster_memory + (guest - ROSTER_BASE);
}

static void roster_u16(uint32_t guest, unsigned value)
{
    roster_at(guest)[0] = (uint8_t)value;
    roster_at(guest)[1] = (uint8_t)(value >> 8);
}

static void roster_u32(uint32_t guest, uint32_t value)
{
    roster_u16(guest, value & 0xFFFFu);
    roster_u16(guest + 2u, value >> 16);
}

/* costume_model_load (0x00086060) and costume_stream_preload (0x000840D0)
   find a model pair at (costume * 25 + char) for costumes below 0x40. Added
   characters 25..49 take costume ids 16 + k, which land past the stock rows
   (17 costumes x 25) without overlapping anything, so both stay lifted.
   shortcut: holds 25 added characters with 48 costumes each; replace both
   loaders with an N-wide stride to go further. */
static uint32_t costume_row(unsigned id, unsigned k)
{
    return COSTUMES + 10u * ((16u + k) * STOCK_CHARS + id);
}

static char *roster_token(char **cursor)
{
    char *start = *cursor + strspn(*cursor, " \t\r\n");
    char *end = start + strcspn(start, " \t\r\n");

    if (*start == '\0') return NULL;
    *cursor = *end != '\0' ? end + 1 : end;
    *end = '\0';
    return start;
}

static void roster_char(char *line, const char *disc_root)
{
    char *cursor = line + 5, *token;
    unsigned id, like = ~0u, k = 0u;
    uint32_t bank[4] = {~0u, ~0u, ~0u, ~0u};

    token = roster_token(&cursor);
    id = token != NULL ? (unsigned)strtoul(token, NULL, 0) : 0u;
    if (id < STOCK_CHARS || id >= ROSTER_CHARS) {
        fprintf(stderr, "recomp roster: char id %u outside %u..%u\n", id, STOCK_CHARS, ROSTER_CHARS - 1u);
        return;
    }
    while ((token = roster_token(&cursor)) != NULL) {
        char *value = strchr(token, '=');
        uint32_t n;

        if (value == NULL) continue;
        *value++ = '\0';
        n = (uint32_t)strtoul(value, NULL, 0);
        if (strcmp(token, "like") == 0 && n < STOCK_CHARS) {
            like = n;
            roster_voice[id] = (uint8_t)n;
            for (size_t t = 0; t < sizeof roster_char_rows / sizeof roster_char_rows[0]; ++t) {
                const uint32_t w = roster_char_rows[t][1];
                memcpy(roster_at(roster_char_rows[t][0] + id * w), roster_at(roster_char_rows[t][0] + like * w), w);
            }
        } else if (strcmp(token, "package") == 0) {
            const uint32_t tables[] = {VARIANT_V0, VARIANT_V1, VARIANT_V4, VARIANT_V2, VARIANT_V3};
            for (size_t t = 0; t < 5u; ++t) roster_u16(tables[t] + 4u * id, n);
        } else if (strcmp(token, "bank") == 0) { bank[0] = n;
        } else if (strcmp(token, "bank1") == 0) { bank[1] = n;
        } else if (strcmp(token, "bank2") == 0) { bank[2] = n;
        } else if (strcmp(token, "bank3") == 0) { bank[3] = n;
        } else if (strcmp(token, "select_bank") == 0) { roster_u16(VARIANT_V4 + 4u * id + 2u, n);
        } else if (strcmp(token, "compact") == 0) { roster_at(CHAR_COMPACT)[id] = (uint8_t)n;
        } else if (strcmp(token, "voice") == 0) { roster_voice[id] = (uint8_t)n;
        } else if (strcmp(token, "clip") == 0) { roster_u16(PREVIEW_CLIP + 2u * id, n);
        } else if (strcmp(token, "param") == 0) { roster_u32(PREVIEW_PARAM + 4u * id, n);
        } else if (strcmp(token, "body") == 0) {
            char path[1024];
            const uint32_t block = BODY_BLOCKS + 0x110u * (id - STOCK_CHARS);
            FILE *file;

            snprintf(path, sizeof path, "%s/%s", disc_root, value);
            file = fopen(path, "rb");
            if (file == NULL || fread(roster_at(block), 1u, 0x110u, file) != 0x110u) {
                fprintf(stderr, "recomp roster: char %u body %s needs 0x110 bytes\n", id, path);
            } else {
                roster_u32(BODY + 4u * id, block);
            }
            if (file != NULL) fclose(file);
        } else if (strcmp(token, "costume") == 0 && k < 48u) {
            const uint32_t row = costume_row(id, k);
            const uint32_t list = PICK_BYTES + 128u * (id - STOCK_CHARS);
            char *col = value;

            for (unsigned c = 0u; c < 5u; ++c) {
                unsigned v = like != ~0u ? roster_at(COSTUMES + 10u * like)[2u * c] |
                    roster_at(COSTUMES + 10u * like)[2u * c + 1u] << 8 : 0xFFFFu;
                if (col != NULL && *col != '\0') {
                    v = (unsigned)strtoul(col, &col, 0);
                    col = *col == ':' ? col + 1 : NULL;
                }
                roster_u16(row + 2u * c, v);
            }
            /* picker: one variant per costume, costume k is id 16 + k */
            roster_at(list)[k] = (uint8_t)(16u + k);
            roster_at(list + 64u)[k] = 1u;
            ++k;
            roster_at(PICK_COUNT)[id] = (uint8_t)k;
            roster_at(PICK_CAP)[id] = (uint8_t)k;
            roster_u32(PICK_LIST + 4u * id, list);
            roster_u32(PICK_OUTFIT + 4u * id, list + 64u);
        }
    }
    for (unsigned v = 0u; v < 4u; ++v) {
        const uint32_t tables[] = {VARIANT_V0, VARIANT_V1, VARIANT_V2, VARIANT_V3};
        const uint32_t b = bank[v] != ~0u ? bank[v] : bank[0];
        if (b != ~0u) roster_u16(tables[v] + 4u * id + 2u, b);
    }
}

RecompMemoryRegion recomp_doa3_roster_map(const uint8_t *ram, const char *disc_root)
{
    char path[1024];
    char line[512];
    FILE *file;

    for (size_t i = 0; i < sizeof roster_relocations / sizeof roster_relocations[0]; ++i) {
        memcpy(roster_at(roster_relocations[i][2]), ram + roster_relocations[i][0], roster_relocations[i][1]);
    }
    /* The lifter moves only CPU_SCRIPT's first 6 rows: 0x1eb40 indexes an
       action table through the base 0x3f5e18 inside it. The one reader
       (0x1a022) indexes from 0x3f5e00, so all 25 rows live in the copy. */
    memcpy(roster_at(CPU_SCRIPT), ram + 0x003F5E00u, 4u * STOCK_CHARS);
    snprintf(path, sizeof path, "%s/roster.txt", disc_root);
    file = fopen(path, "r");
    while (file != NULL && fgets(line, sizeof line, file) != NULL) {
        unsigned v[6];

        if (strncmp(line, "char ", 5) == 0) {
            roster_char(line, disc_root);
        } else if (sscanf(line, "sprite %i %i %i %i %i %i", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) == 6 &&
                   v[0] < SPRITE_MAX) {
            for (unsigned c = 0u; c < 5u; ++c) roster_u16(SPRITES + 12u * v[0] + 2u * c, v[1 + c]);
        } else if (sscanf(line, "grid %i %i %i %i %i %i", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) == 6 &&
                   grid_records < GRID_RECORD_MAX) {
            const uint32_t record = GRID_RECORDS + 10u * grid_records++;
            roster_at(record)[0] = (uint8_t)v[0];
            roster_at(record)[1] = (uint8_t)v[1];
            for (unsigned c = 0u; c < 4u; ++c) roster_u16(record + 2u + 2u * c, v[2 + c]);
        }
    }
    if (file != NULL) {
        fclose(file);
        fprintf(stderr, "recomp roster: %u grid records from %s\n", grid_records, path);
    }
    return (RecompMemoryRegion){ROSTER_BASE, ROSTER_SIZE, roster_memory};
}

static uint8_t grid_char(unsigned record)
{
    return roster_at(GRID_RECORDS)[10u * record];
}

/* charsel_build_grid (0x000CD9E0), mode in EAX: the stock body builds the
   16-cell grid (Ein locked, or mode 0) or the 18-cell grid, the char -> cell
   map and the costume-unlock merge. With added records, rebuild the cells as
   every fighter record in order (Ein only when unlocked), then RANDOM, padded
   with a second RANDOM to an even count: the cursor and draw code lay the grid
   out as two rows of count / 2, and RANDOM resolves to the last cell.
   shortcut: two rows of 68 px cells fill the 720 px frame at 20 cells, and
   stock code clears only 18 cell offsets; replace the grid layout code to go
   past 20. */
extern void sub_000CD9E0_gen(void);
void sub_000CD9E0(void)
{
    const uint32_t mode = recomp_runtime.registers.eax;
    const bool ein = mode != 0u && *recomp_memory(EIN_UNLOCKED, 1u) != 0u;
    uint8_t *slots = roster_at(GRID_SLOTS);
    unsigned count = 0u, random = GRID_RECORD_MAX;

    sub_000CD9E0_gen();
    if (grid_records == GRID_STOCK_RECORDS) return;
    for (unsigned r = 0u; r < grid_records; ++r) {
        if (grid_char(r) == CHAR_RANDOM) { random = r; continue; }
        if (grid_char(r) == CHAR_EIN && !ein) continue;
        slots[count++] = (uint8_t)r;
    }
    if (random != GRID_RECORD_MAX) {
        slots[count++] = (uint8_t)random;
        if (count & 1u) slots[count++] = (uint8_t)random;
    }
    *recomp_memory_u32(GRID_COUNT) = count;
    for (unsigned c = 0u; c < ROSTER_CHARS; ++c) {
        uint8_t cell = 0u;
        for (unsigned i = 0u; i < count && (ein || c != CHAR_EIN); ++i) {
            if (grid_char(slots[i]) == c) { cell = (uint8_t)i; break; }
        }
        roster_at(CHAR_CELL)[c] = cell;
    }
}

/* Select-screen helpers that return early for ids above 24 (the costume
   picker and the char -> cell lookups). For an added character the call runs
   on CHAR_PROXY with the two characters' rows swapped, then swapped back;
   none of these yields. charsel_build_grid's callers (0x000CAAA0,
   0x000CADD0) inline the same check when they restore a cursor: an added
   character's cursor starts on the last cell. */
static void roster_proxy_call(void (*gen)(void), uint32_t *reg)
{
    static const uint32_t rows[][2] = {
        {CHAR_CELL, 1u}, {CHAR_COMPACT, 1u}, {PICK_COUNT, 1u}, {PICK_CAP, 1u},
        {PICK_LIST, 4u}, {PICK_OUTFIT, 4u},
    };
    const unsigned id = *reg & 0xFFu;

    if (id < STOCK_CHARS || id >= ROSTER_CHARS) { gen(); return; }
    for (int pass = 0; pass < 2; ++pass) {
        for (size_t t = 0; t < sizeof rows / sizeof rows[0]; ++t) {
            uint8_t *a = roster_at(rows[t][0] + id * rows[t][1]);
            uint8_t *b = roster_at(rows[t][0] + CHAR_PROXY * rows[t][1]);
            for (uint32_t i = 0u; i < rows[t][1]; ++i) { uint8_t s = a[i]; a[i] = b[i]; b[i] = s; }
        }
        if (pass == 0) {
            *reg = (*reg & ~0xFFu) | CHAR_PROXY;
            gen();
        }
    }
}

extern void sub_000CD870_gen(void);   /* charsel_costume_pick, char in AL */
void sub_000CD870(void) { roster_proxy_call(sub_000CD870_gen, &recomp_runtime.registers.eax); }
extern void sub_000CD900_gen(void);   /* costume -> picker entry, char in CL */
void sub_000CD900(void) { roster_proxy_call(sub_000CD900_gen, &recomp_runtime.registers.ecx); }
extern void sub_000CD7C0_gen(void);   /* picker entry count, char in AL */
void sub_000CD7C0(void) { roster_proxy_call(sub_000CD7C0_gen, &recomp_runtime.registers.eax); }
extern void sub_000CD110_gen(void);   /* picker variant, char in DL */
void sub_000CD110(void) { roster_proxy_call(sub_000CD110_gen, &recomp_runtime.registers.edx); }
extern void sub_000CD820_gen(void);   /* char -> cell, char in AL */
void sub_000CD820(void) { roster_proxy_call(sub_000CD820_gen, &recomp_runtime.registers.eax); }
extern void sub_000CEF60_gen(void);   /* charsel_cell_field, char in CL */
void sub_000CEF60(void) { roster_proxy_call(sub_000CEF60_gen, &recomp_runtime.registers.ecx); }

/* FUN_00085e40 (load_queue_run's per-fighter voice bank, char in the byte
   at [esp + 8]): a char with a record in 0x3092f4 loads its 0x404b1c bank
   file and registers it (FUN_00071f20). Ids above 24 and ids without a
   record take a path no stock fighter reaches: it loads loadfile entry
   <char> to guest address <char> (entry 25 overwrote code) and then a file
   named by uninitialised stack words (FUN_00085f70). Run it as the added
   character's voice= instead, a stock fighter with a record.
   shortcut: added characters borrow a stock voice bank. */
extern void sub_00085E40_gen(void);
void sub_00085E40(void)
{
    uint8_t *arg = recomp_memory(recomp_runtime.registers.esp + 8u, 1u);
    const uint8_t id = *arg;

    if (id >= STOCK_CHARS && id < ROSTER_CHARS) *arg = roster_voice[id];
    sub_00085E40_gen();
    *arg = id;
}


/* Japan entry point -> the address its adapter is keyed by. Only these reach
   the adapters: other USA keys can be unrelated Japan functions. */
static const uint32_t jp_manual[][2] = {
    {0x0018c934u, 0x00164f50u},
    {0x0018c9c0u, 0x00164fdcu},
    {0x0018c9d3u, 0x00164fefu},
    {0x0018ca12u, 0x0016502eu},
    {0x001926f0u, 0x0016a570u},
    {0x00192760u, 0x0016a5e0u},
    {0x001927d0u, 0x0016a650u},
    {0x001a3480u, 0x001779d0u},
    {0x001dc360u, 0x001b0e00u},
    {0x001dc660u, 0x001b1130u},
    {0x001dd670u, 0x001b1cc0u},
    {0x001dd880u, 0x001b1ea0u},
    {0x001dd950u, 0x001b1f30u},
    {0x001ddeb0u, 0x001b2390u},
    {0x001de080u, 0x001b2470u},
    {0x001de150u, 0x001b2520u},
    {0x001de200u, 0x001b25d0u},
    {0x001de240u, 0x001b2600u},
    {0x001de470u, 0x001b2800u},
    {0x001dec40u, 0x001b3040u},
    {0x001dece0u, 0x001b30b0u},
    {0x001ded70u, 0x001b3130u},
    {0x001def50u, 0x001b32f0u},
    {0x001df010u, 0x001b3390u},
    {0x001df490u, 0x001b3760u},
    {0x001df5f0u, 0x001b38a0u},
    {0x001df690u, 0x001b3940u},
    {0x001dff50u, 0x001b4230u},
    {0x001e0310u, 0x001b45f0u},
    {0x001e14d0u, 0x001e14d0u},
    {0x001e2790u, 0x001b8890u},
    {0x001e27c0u, 0x001b88c0u},
    {0x001e4758u, 0x001ba7d8u},
    {0x001e5283u, 0x001bb1f3u},
    {0x001e52e6u, 0x001bb256u},
    {0x001e5623u, 0x001bb595u},
    {0x001e56f2u, 0x001bb66au},
    {0x001e57edu, 0x001bb770u},
    {0x001e594fu, 0x001bb8a8u},
    {0x001e5b3du, 0x001bba96u},
    {0x001f13a5u, 0x001c6b72u},
    {0x001f13aau, 0x001c6b77u},
    {0x001f13afu, 0x001c6b7cu},
    {0x001f13c5u, 0x001c6b92u},
    {0x001f1d46u, 0x001c7392u},
    {0x001f1d62u, 0x001c73aeu},
    {0x001f1d89u, 0x001c7477u},
    {0x001f1da5u, 0x001f1da5u},
    {0x001f1dc1u, 0x001c7b17u},
    {0x001f1dddu, 0x001c7493u},
    {0x001f1df9u, 0x001c7b33u},
    {0x001f1e1du, 0x001c7b57u},
    {0x001f1e35u, 0x001c7541u},
    {0x001f1e59u, 0x001c7565u},
    {0x001f1e79u, 0x001c7585u},
    {0x001f1e95u, 0x001c75a1u},
    {0x001f1eb5u, 0x001c75c1u},
    {0x001f1ed1u, 0x001c75ddu},
    {0x001f1f01u, 0x001c760du},
    {0x001f23ccu, 0x001c743fu},
    {0x001f23e4u, 0x001c7afbu},
    {0x001f2400u, 0x001c74b3u},
    {0x001f2424u, 0x001c74d7u},
    {0x001f2459u, 0x001c750cu},
    {0x001f29bdu, 0x001c73d5u},
    {0x001f29f2u, 0x001c740au},
    {0x001f2a27u, 0x001c7457u},
    {0x001f2a47u, 0x001c7b6fu},
    {0x001f2cabu, 0x001c7ea9u},
    {0x001f2e65u, 0x001c7fd9u},
    {0x00210b6cu, 0x001e5d4cu},
    {0x00211578u, 0x001e6953u},
    {0x0021157du, 0x001e6958u},
    {0x0021159fu, 0x001e697au},
    {0x00211823u, 0x001e6e3au},
    {0x00211898u, 0x001e6eafu},
    {0x002118a4u, 0x001e6ebbu},
    {0x00211a96u, 0x001e70adu},
    {0x00211b07u, 0x001e711eu},
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
