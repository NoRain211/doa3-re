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

#include <assert.h>
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
       compact=ID                 share stock profile ID (voice set, records,
                                  unlocks) instead of its own profile
       voice_tag=T                its own voice tag byte: the high byte of
                                  its sound ids (see roster_voice_retag)
       voice_bank=E               loadfile entry (0x406 or above) of its
                                  in-match voice bank
       voice_like=ID              take its voice-line rows from stock char ID
                                  (before voice_tag retags them; default like)
       voice_stream=A:B           voice.afs entries A..B exist (streamed lines,
                                  ids 0x8000 | n; see VOICE_STREAMS)
       name_strip=ID|ENTRY        life-bar name set: a stock fighter's (ID
                                  below 25, default like) or loadfile ENTRY
                                  (25 and up), see 0x00085e40
       name_sprite=S              sprite of its life-bar name; S + 1 is the
                                  set's icon (sprite rows {0,0,w,16} 0x43 and
                                  {0,0,32,32} 0x44; default like's)
       face=U:V                   its profile face (records screens) is the
                                  80x48 cell at U,V of face sheet #2, see
                                  roster_face (default like's face)
       ending=FILE|none           its own Story ending movie, d:\FILE
       clip= param=               select-screen preview clip and parameter
       body=FILE                  0x110-byte body block beside roster.txt
       costume=M1:M0[:C2:C3:C4]   one per costume, in picker order; missing
                                  columns come from like's first costume
     grid CHAR KIND BIG SMALL NAME STYLE   a select cell (sprite ids)

   Profiles. Stock fighters share 19 profiles ("compact" ids, 0x2ff868):
   voice set, records, rankings, unlocks. Added character 25 + k owns
   profile 19 + k, a copy of like's profile. The profile tables below move
   to the region with room for 50 profiles; the ones inside the save block
   (0x59d088, 0x3c34 bytes, written whole as T:\DOA3SAVE) are synced with it
   around the save write and load, and their rows 19+ live in the sidecar
   .recomp-storage/roster-profiles-v1 (see roster_save_sync). */
#define ROSTER_BASE 0x04000000u
#define ROSTER_SIZE 0x00040000u
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
/* Per-character tables indexed by the char byte of a fighter or CPU-AI
   record (+1), found in the round-7 bounds sweep. */
#define CHAR_220468 R(0x18600u)  /* 0x19220 */
#define CHAR_259C78 R(0x18700u)  /* 0x181910 0x182080 */
#define CHAR_259CAC R(0x18800u)  /* 0x181910 0x182080 */
#define CHAR_26C6C0 R(0x18900u)  /* 0x185070 */
#define CHAR_26C930 R(0x18A00u)  /* 0x14d50 0x181910 0x182080 */
#define CHAR_26C964 R(0x18B00u)  /* 0x181910 0x182080 */
#define CHAR_2DC9F0 R(0x18C00u)  /* 0x92c90 0xad713 */
#define CHAR_2DD418 R(0x18D00u)  /* 0x90a50 */
#define CHAR_2DD434 R(0x18E00u)  /* 0x90a50 */
#define CHAR_2DD450 R(0x18F00u)  /* 0x90a50 */
#define CHAR_2DD46C R(0x19000u)  /* 0x90a50 */
#define CHAR_303748 R(0x19100u)  /* 0x1405c0 0x72f50 */
#define CHAR_31C3B8 R(0x19200u)  /* 0x4ed10 */
#define CHAR_3E3CC8 R(0x19300u)  /* 0x25640 */
#define CHAR_3F6204 R(0x19400u)  /* 0x14700 */
#define CHAR_3F6238 R(0x19500u)  /* 0x14d50 */
#define CHAR_NAME_SPRITE R(0x19600u) /* u16 life-bar name sprite (+1 icon), 0x6a870 0x6da80 */
/* Sound ids of a character's own voice lines, read by the motion-script
   interpreter 0x8f290 at (char * 3 + n) and (char * 6 + n); 0x2dcc04 is
   the second half of 0x2dcbf8's rows. */
#define CHAR_2DCAC8 R(0x21E00u)
#define CHAR_2DCBF8 R(0x22100u)
/* Profile tables, 50 rows each. */
#define PROF_VOICE_TAG R(0x20000u)   /* u16 tag << 8: 75a40 78b50 7d170 7dab0 7e980 851a0 86880 86910 */
#define PROF_VOICE_FILE R(0x20100u)  /* {u16 voice bank entry - 0x406, 0}: via 0x5b1488, 80a80 85850 */
#define PROF_NAME_SPRITE R(0x20200u) /* u16 sprite id, 0x5b1c0 */
#define PROF_NAME0 R(0x20300u)       /* name pointers: 5bde0 5dd40 5f440 600a0 62be0 63860 653e0 65ec0 67670 */
#define PROF_NAME1 R(0x20400u)
#define PROF_NAMES R(0x20500u)       /* 5bde0 64520 d0b10 */
#define PROF_PREVIEW R(0x20600u)     /* 8 B, d40c0 */
#define PROF_VS_SPRITE R(0x20800u)   /* u16 sprite id, d08a0 */
#define PROF_303B78 R(0x20900u)      /* 8 B, 7e8c0 */
#define PROF_302F88 R(0x20B00u)      /* 0x38 B, 76e50 */
#define PROF_PAIRS R(0x21600u)       /* u16 [profile * 19 + profile], 7dee0 85bf0 */
#define PROF_PAIR_DEST R(0x2D000u)   /* u32 offset of the pair file (PROF_PAIRS) in its buffer, 85bf0 */
/* u32 ending movie (index into MOVIE_NAMES, 0xffffffff = none), 83600. Added
   profiles get none unless roster.txt names a file (ending=); then the index
   is the profile and MOVIE_NAMES[profile] points at its path in MOVIE_PATHS. */
#define PROF_ENDING R(0x2D100u)
#define MOVIE_NAMES R(0x2D200u)      /* u32 name pointers, 0x2d4dd8 (19 stock), read by a47e0 */
#define MOVIE_PATHS R(0x2D300u)      /* 32 B path "d:\<file>" per added profile */
#define STOCK_MOVIES 19u
/* Found by the stride sweep (roster2 NOTES.md round 10). */
#define CHAR_C59158 R(0x2D700u)      /* u32, zero; select-box light scale, bc0d0 */
#define CHAR_2CE260 R(0x2D800u)      /* 2 x u32 angle limits, ad680 */
#define PROF_3033C8 R(0x2DA00u)      /* u32 pointer, Story setup 76e50 */
#define PROF_303428 R(0x2DB00u)      /* u32 pointer, Story setup 76e50 */
/* u32 -> costume unlock conditions (2 per tier), 8c7e0; it runs only while
   the char's costume count (PICK_CAP) exceeds its tier. shortcut: an added
   profile uses like's list, so costumes beyond like's count read the next
   profile's conditions. */
#define PROF_2FEF30 R(0x2DC00u)
#define SELECT_LIGHT0 R(0x30000u)    /* 0x160 B light set, select box idle: bc0d0 bc450 */
#define SELECT_LIGHT1 R(0x34800u)    /* 0x160 B light set, select box other state */
#define SAVE_REC0 R(0x22600u)        /* save block: u16 records */
#define SAVE_REC1 R(0x22700u)
#define SAVE_REC2 R(0x22800u)
#define SAVE_VS R(0x22900u)          /* u16 [profile * 19 + opponent profile], 20bd0 */
#define SAVE_TIER R(0x23100u)        /* costume tiers earned (see recomp_save_unlock_all) */
#define SAVE_U0 R(0x23200u)          /* u8 unlock counters */
#define SAVE_U1 R(0x23300u)
#define SAVE_U2 R(0x23400u)
#define SAVE_RK0 R(0x23500u)         /* 0x6c B ranking rows */
#define SAVE_RK1 R(0x24B00u)
#define SAVE_RK2 R(0x26100u)         /* 0x78 B ranking rows */
#define SAVE_RK3 R(0x27900u)
#define PROF_XCOUNT R(0x29100u)      /* extra costumes (ids 0x40+): count, 86ad0 cd7c0 cd870 cd900 */
#define PROF_XCOSTUME R(0x29200u)    /* 7 x u32 per profile, 840d0 86060 86ad0 86cc0 */
/* voice.afs entry -> name pointer; 0x001c70a0 streams entry n only when it
   is non-null (and n < 0x370). voice_stream= marks added entries. */
#define VOICE_STREAMS R(0x29800u)
#define VOICE_STREAM_COUNT 0x370u
/* Tag-team match counts, u16 per pair of chars (save block 0x59f6e6: the
   171 stock slots); pairs with an added char follow, see sub_0007A330.
   SAVE_TAG_WINS holds only the added pairs' counts of the save block's
   0x59fb4e (see sub_00020BD0). */
#define SAVE_TAG R(0x2B000u)
#define TAG_STOCK_SLOTS 171u
#define TAG_ADDED_SLOTS ((ROSTER_CHARS - STOCK_CHARS) * ROSTER_CHARS)
#define SAVE_TAG_WINS R(0x2C000u)

#define STOCK_CHARS 25u
#define ROSTER_CHARS 50u           /* see costume_row */
#define STOCK_PROFILES 19u
#define ROSTER_PROFILES 50u
#define VOICE_BANK_BASE 0x406u     /* load_queue_run loads entry 0x406 + file */
#define CHAR_PROXY 23u             /* CHIN: no select cell, no picker entries */
#define GRID_RECORD_MAX 0xFDu      /* char ids 0xFE/0xFF are special */
#define GRID_STOCK_RECORDS 18u
#define GRID_TWO_ROWS 20u          /* the stock layout fits two rows of 10 */
#define GRID_COUNT 0x0029CB14u
#define EIN_UNLOCKED 0x005A0B47u
#define CHAR_EIN 3u
#define CHAR_RANDOM 0xFFu
#define SPRITE_MAX ((BODY_BLOCKS - SPRITES) / 12u)
/* Profile faces. A profile's PROF_NAME0 / PROF_NAME1 rows are base sprite
   ids of its 60 px and 80 px face; the records screens (653e0 65ec0 67670:
   base - 0x68), 5bde0 5f440 62be0 (base), 600a0 63860 (base + 0x4e) and
   5dd40 (base + 0x4e or 0xa5) each draw from their own copy of the three
   face sheets. face= gives an added profile eight rows from FACE_SPRITES on,
   copied from profile 3's (a sheet #2 face) and moved to its cell.
   shortcut: sheet #2 only (the sheet face_sheet.py grows); roster.txt
   sprite lines must stay below FACE_SPRITES. */
#define FACE_SPRITES 0x900u
#define FACE_REF_PROFILE 3u
static const int face_offsets[] = {-0x68, 0, 0x4e, 0xa5};
_Static_assert(FACE_SPRITES + 2u * (ROSTER_PROFILES - STOCK_PROFILES) + 0x68u + 0xa5u < SPRITE_MAX,
               "face sprites overflow the sprite table");

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
    {0x00220468u, 0x0032u, CHAR_220468},
    {0x00259C78u, 0x0032u, CHAR_259C78},
    {0x00259CACu, 0x0032u, CHAR_259CAC},
    {0x0026C6C0u, 0x0064u, CHAR_26C6C0},
    {0x0026C930u, 0x0032u, CHAR_26C930},
    {0x0026C964u, 0x0032u, CHAR_26C964},
    {0x002DC9F0u, 0x0064u, CHAR_2DC9F0},
    {0x002DD418u, 0x0019u, CHAR_2DD418},
    {0x002DD434u, 0x0019u, CHAR_2DD434},
    {0x002DD450u, 0x0019u, CHAR_2DD450},
    {0x002DD46Cu, 0x0019u, CHAR_2DD46C},
    {0x00303748u, 0x0064u, CHAR_303748},
    {0x0031C3B8u, 0x0064u, CHAR_31C3B8},
    {0x003E3CC8u, 0x0064u, CHAR_3E3CC8},
    {0x003F6204u, 0x0032u, CHAR_3F6204},
    {0x003F6238u, 0x0032u, CHAR_3F6238},
    {0x00309300u, 0x0040u, CHAR_NAME_SPRITE},
    {0x00302C2Cu, 0x0026u, PROF_VOICE_TAG},
    {0x002D3E30u, 0x004Cu, PROF_VOICE_FILE},
    {0x00304410u, 0x0026u, PROF_NAME_SPRITE},
    {0x00303E08u, 0x004Cu, PROF_NAME0},
    {0x00303E58u, 0x004Cu, PROF_NAME1},
    {0x00303DB8u, 0x004Cu, PROF_NAMES},
    {0x0029BE98u, 0x0098u, PROF_PREVIEW},
    {0x0029BDD0u, 0x0026u, PROF_VS_SPRITE},
    {0x00303B78u, 0x0098u, PROF_303B78},
    {0x00302F88u, 0x0428u, PROF_302F88},
    {0x00302908u, 0x02D2u, PROF_PAIRS},
    {0x00302BE0u, 0x004Cu, PROF_PAIR_DEST},
    {0x00302DC8u, 0x004Cu, PROF_ENDING},
    {0x002D4DD8u, 0x004Cu, MOVIE_NAMES},
    {0x002DCAC8u, 0x012Cu, CHAR_2DCAC8},
    {0x002DCBF8u, 0x0258u, CHAR_2DCBF8},
    {0x0059F674u, 0x0026u, SAVE_REC0},
    {0x0059F69Au, 0x0026u, SAVE_REC1},
    {0x0059F6C0u, 0x0026u, SAVE_REC2},
    {0x0059F83Cu, 0x02D2u, SAVE_VS},
    {0x0059FB16u, 0x0013u, SAVE_TIER},
    {0x005A0B49u, 0x0013u, SAVE_U0},
    {0x005A0C92u, 0x0013u, SAVE_U1},
    {0x005A0CA5u, 0x0013u, SAVE_U2},
    {0x0059D2BCu, 0x0804u, SAVE_RK0},
    {0x0059DAC0u, 0x0804u, SAVE_RK1},
    {0x0059E4A4u, 0x08E8u, SAVE_RK2},
    {0x0059ED8Cu, 0x08E8u, SAVE_RK3},
    {0x005B3174u, 0x0013u, PROF_XCOUNT},
    {0x005B1E58u, 0x0214u, PROF_XCOSTUME},
    {0x002D8378u, 0x0DC0u, VOICE_STREAMS},
    {0x0059F6E6u, 0x0156u, SAVE_TAG},
    {0x00C59158u, 0x0064u, CHAR_C59158},
    {0x002CE260u, 0x00C8u, CHAR_2CE260},
    {0x003033C8u, 0x004Cu, PROF_3033C8},
    {0x00303428u, 0x004Cu, PROF_303428},
    {0x002FEF30u, 0x004Cu, PROF_2FEF30},
    {0x002C36E8u, 0x2260u, SELECT_LIGHT0},
    {0x002C5A38u, 0x2260u, SELECT_LIGHT1},
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
    {CHAR_2DCAC8, 12u}, {CHAR_2DCBF8, 24u},
    {CHAR_220468, 2u}, {CHAR_259C78, 2u}, {CHAR_259CAC, 2u}, {CHAR_26C6C0, 4u},
    {CHAR_26C930, 2u}, {CHAR_26C964, 2u}, {CHAR_2DC9F0, 4u}, {CHAR_2DD418, 1u},
    {CHAR_2DD434, 1u}, {CHAR_2DD450, 1u}, {CHAR_2DD46C, 1u}, {CHAR_303748, 4u},
    {CHAR_31C3B8, 4u}, {CHAR_3E3CC8, 4u}, {CHAR_3F6204, 2u}, {CHAR_3F6238, 2u}, {CHAR_NAME_SPRITE, 2u},
    {CHAR_C59158, 4u}, {CHAR_2CE260, 8u}, {SELECT_LIGHT0, 0x160u}, {SELECT_LIGHT1, 0x160u},
};
/* Per-profile rows copied from like's profile: table, bytes per profile.
   PROF_PAIRS keeps the stock stride of 19 columns: its reader sub_00085BF0
   runs an added column profile as like's.
   shortcut: SAVE_VS keeps the stride of 19 too, so a pair of two added
   profiles shares a cell with the next row. */
static const uint32_t roster_profile_rows[][2] = {
    {PROF_VOICE_TAG, 2u}, {PROF_VOICE_FILE, 4u}, {PROF_NAME_SPRITE, 2u}, {PROF_NAME0, 4u},
    {PROF_NAME1, 4u}, {PROF_NAMES, 4u}, {PROF_PREVIEW, 8u}, {PROF_VS_SPRITE, 2u},
    {PROF_303B78, 8u}, {PROF_302F88, 0x38u}, {PROF_PAIRS, 0x26u}, {PROF_PAIR_DEST, 4u},
    {PROF_3033C8, 4u}, {PROF_303428, 4u}, {PROF_2FEF30, 4u},
};
/* Save-block tables: stock address, bytes in the block, copy, bytes in the
   copy (the rest goes to the sidecar). */
#define PROFILE_ROWS(address, width, copy) {address, STOCK_PROFILES * (width), copy, ROSTER_PROFILES * (width)}
static const uint32_t roster_save_rows[][4] = {
    PROFILE_ROWS(0x0059F674u, 2u, SAVE_REC0), PROFILE_ROWS(0x0059F69Au, 2u, SAVE_REC1),
    PROFILE_ROWS(0x0059F6C0u, 2u, SAVE_REC2), PROFILE_ROWS(0x0059F83Cu, 0x26u, SAVE_VS),
    PROFILE_ROWS(0x0059FB16u, 1u, SAVE_TIER), PROFILE_ROWS(0x005A0B49u, 1u, SAVE_U0),
    PROFILE_ROWS(0x005A0C92u, 1u, SAVE_U1), PROFILE_ROWS(0x005A0CA5u, 1u, SAVE_U2),
    PROFILE_ROWS(0x0059D2BCu, 0x6Cu, SAVE_RK0), PROFILE_ROWS(0x0059DAC0u, 0x6Cu, SAVE_RK1),
    PROFILE_ROWS(0x0059E4A4u, 0x78u, SAVE_RK2), PROFILE_ROWS(0x0059ED8Cu, 0x78u, SAVE_RK3),
    {0x0059F6E6u, 2u * TAG_STOCK_SLOTS, SAVE_TAG, 2u * (TAG_STOCK_SLOTS + TAG_ADDED_SLOTS)},
    {0x0059FB4Eu, 0u, SAVE_TAG_WINS, 2u * TAG_ADDED_SLOTS},
};
static uint8_t roster_memory[ROSTER_SIZE];
static unsigned grid_records = GRID_STOCK_RECORDS;
static uint8_t roster_name_strip[ROSTER_CHARS];  /* added character -> name_strip= */
static uint16_t roster_name_file[ROSTER_CHARS];  /* added character -> name_strip=ENTRY (0 = none) */
static uint8_t profile_like[ROSTER_PROFILES];    /* added profile -> like's profile */
static uint8_t profile_char[ROSTER_PROFILES];    /* added profile -> its char (0 = not added) */
static void rank_defaults(size_t first, size_t count, bool only_empty);
static void rank_swap(unsigned p);
static unsigned rank_session;
static void rank_session_end(void);
static bool roster_added;                        /* roster.txt added a character */
static char roster_root[1024];

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

/* A fighter's voice lines are sound ids tag << 8 | clip. load_queue_run
   (0x000851a0) loads the voice bank, entry 0x406 + PROF_VOICE_FILE, into a
   sound slot and marks the slot with PROF_VOICE_TAG; 0x000a5940 plays an id
   from the slot whose mark equals id & 0xff00. Ids 0x1000..0x2fff are
   voices: the voice option mutes them (0x0008ee60) and 0x000a5940 stops
   the player's previous line first. Stock tags are 0x10..0x23 except 0x18,
   0x1b and 0x1d; added characters take one of those three or 0x24..0x2f.
   This retags like's voice-line rows; the package's motion scripts must
   carry the same tag (its builder's job). */
static void roster_voice_retag(unsigned id, unsigned tag)
{
    const uint32_t rows[][2] = {{CHAR_2DCAC8 + 12u * id, 3u}, {CHAR_2DCBF8 + 24u * id, 6u}};

    for (size_t t = 0; t < 2u; ++t) {
        for (uint32_t i = 0u; i < rows[t][1]; ++i) {
            uint8_t *v = roster_at(rows[t][0] + 4u * i);
            if (v[2] == 0u && v[3] == 0u && v[1] >= 0x10u && v[1] < 0x30u) v[1] = (uint8_t)tag;
        }
    }
}

/* Point an added profile's face at the 80x48 cell (u, v) of sheet #2. */
static void roster_face(unsigned profile, unsigned u, unsigned v)
{
    const uint32_t tables[2] = {PROF_NAME0, PROF_NAME1};

    for (unsigned f = 0u; f < 2u; ++f) {
        const uint32_t base = FACE_SPRITES + 0x68u + 2u * (profile - STOCK_PROFILES) + f;
        const uint8_t *ref = roster_at(tables[f] + 4u * FACE_REF_PROFILE);
        const uint32_t ref_base = ref[0] | ref[1] << 8;

        for (size_t i = 0; i < sizeof face_offsets / sizeof face_offsets[0]; ++i) {
            const uint8_t *src = roster_at(SPRITES + 12u * (uint32_t)((int)ref_base + face_offsets[i]));
            const uint32_t dst = SPRITES + 12u * (uint32_t)((int)base + face_offsets[i]);
            const unsigned u0 = src[0] | src[1] << 8, v0 = src[2] | src[3] << 8;
            const unsigned u1 = src[4] | src[5] << 8, v1 = src[6] | src[7] << 8;

            memcpy(roster_at(dst), src, 12u);
            roster_u16(dst, u + u0 % 80u);
            roster_u16(dst + 2u, v + v0 % 48u);
            roster_u16(dst + 4u, u + u0 % 80u + (u1 - u0));
            roster_u16(dst + 6u, v + v0 % 48u + (v1 - v0));
        }
        roster_u32(tables[f] + 4u * profile, base);
    }
}

static void roster_char(char *line, const char *disc_root)
{
    char *cursor = line + 5, *token;
    unsigned id, like = ~0u, k = 0u, voice_tag = 0u, voice_bank = 0u, voice_like = ~0u, profile;
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
            const unsigned own = STOCK_PROFILES + (id - STOCK_CHARS);
            const unsigned like_profile = roster_at(CHAR_COMPACT)[n];
            like = n;
            roster_name_strip[id] = (uint8_t)n;
            for (size_t t = 0; t < sizeof roster_char_rows / sizeof roster_char_rows[0]; ++t) {
                const uint32_t w = roster_char_rows[t][1];
                memcpy(roster_at(roster_char_rows[t][0] + id * w), roster_at(roster_char_rows[t][0] + like * w), w);
            }
            for (size_t t = 0; t < sizeof roster_profile_rows / sizeof roster_profile_rows[0]; ++t) {
                const uint32_t w = roster_profile_rows[t][1];
                memcpy(roster_at(roster_profile_rows[t][0] + own * w),
                       roster_at(roster_profile_rows[t][0] + like_profile * w), w);
            }
            roster_at(CHAR_COMPACT)[id] = (uint8_t)own;
            profile_like[own] = (uint8_t)like_profile;
            profile_char[own] = (uint8_t)id;
            roster_u32(PROF_ENDING + 4u * own, 0xFFFFFFFFu);
        } else if (strcmp(token, "ending") == 0 && roster_at(CHAR_COMPACT)[id] >= STOCK_PROFILES) {
            const unsigned own = roster_at(CHAR_COMPACT)[id];   /* needs like= first */
            const uint32_t path = MOVIE_PATHS + 32u * (own - STOCK_PROFILES);

            if (strcmp(value, "none") != 0) {
                snprintf((char *)roster_at(path), 32u, "d:\\%s", value);
                roster_u32(MOVIE_NAMES + 4u * own, path);
                roster_u32(PROF_ENDING + 4u * own, own);
            }
        } else if (strcmp(token, "package") == 0) {
            const uint32_t tables[] = {VARIANT_V0, VARIANT_V1, VARIANT_V4, VARIANT_V2, VARIANT_V3};
            for (size_t t = 0; t < 5u; ++t) roster_u16(tables[t] + 4u * id, n);
        } else if (strcmp(token, "bank") == 0) { bank[0] = n;
        } else if (strcmp(token, "bank1") == 0) { bank[1] = n;
        } else if (strcmp(token, "bank2") == 0) { bank[2] = n;
        } else if (strcmp(token, "bank3") == 0) { bank[3] = n;
        } else if (strcmp(token, "select_bank") == 0) { roster_u16(VARIANT_V4 + 4u * id + 2u, n);
        } else if (strcmp(token, "compact") == 0 && n < STOCK_PROFILES) { roster_at(CHAR_COMPACT)[id] = (uint8_t)n;
        } else if (strcmp(token, "voice_tag") == 0) { voice_tag = n;
        } else if (strcmp(token, "voice_bank") == 0) { voice_bank = n;
        } else if (strcmp(token, "voice_like") == 0 && n < STOCK_CHARS) { voice_like = n;
        } else if (strcmp(token, "voice_stream") == 0) {
            const char *last = strchr(value, ':');
            const uint32_t end = last != NULL ? (uint32_t)strtoul(last + 1, NULL, 0) : n;
            uint32_t present = 0u;

            for (uint32_t e = 0u; e < VOICE_STREAM_COUNT && present == 0u; ++e) {
                memcpy(&present, roster_at(VOICE_STREAMS + 4u * e), 4u);
            }
            for (uint32_t e = n; e <= end && e < VOICE_STREAM_COUNT; ++e) {
                uint32_t name;
                memcpy(&name, roster_at(VOICE_STREAMS + 4u * e), 4u);
                if (name == 0u) roster_u32(VOICE_STREAMS + 4u * e, present);
            }
        } else if (strcmp(token, "name_strip") == 0) {
            if (n < STOCK_CHARS) roster_name_strip[id] = (uint8_t)n;
            else roster_name_file[id] = (uint16_t)n;
        } else if (strcmp(token, "name_sprite") == 0) { roster_u16(CHAR_NAME_SPRITE + 2u * id, n);
        } else if (strcmp(token, "face") == 0 && roster_at(CHAR_COMPACT)[id] >= STOCK_PROFILES &&
                   strchr(value, ':') != NULL) {
            roster_face(roster_at(CHAR_COMPACT)[id], n, (unsigned)strtoul(strchr(value, ':') + 1, NULL, 0));
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
    roster_added = true;
    if (voice_like != ~0u) {
        memcpy(roster_at(CHAR_2DCAC8 + 12u * id), roster_at(CHAR_2DCAC8 + 12u * voice_like), 12u);
        memcpy(roster_at(CHAR_2DCBF8 + 24u * id), roster_at(CHAR_2DCBF8 + 24u * voice_like), 24u);
    }
    profile = roster_at(CHAR_COMPACT)[id];
    if ((voice_tag != 0u || voice_bank != 0u) && profile < STOCK_PROFILES) {
        fprintf(stderr, "recomp roster: char %u shares stock profile %u; voice_tag/voice_bank ignored\n", id, profile);
        return;
    }
    if (voice_tag != 0u) {
        roster_u16(PROF_VOICE_TAG + 2u * profile, (voice_tag & 0xFFu) << 8);
        roster_voice_retag(id, voice_tag & 0xFFu);
    }
    if (voice_bank >= VOICE_BANK_BASE) roster_u32(PROF_VOICE_FILE + 4u * profile, voice_bank - VOICE_BANK_BASE);
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
    /* added profiles start as a new save does (0x000208e0: two tiers) */
    memset(roster_at(SAVE_TIER) + STOCK_PROFILES, 2, ROSTER_PROFILES - STOCK_PROFILES);
    snprintf(roster_root, sizeof roster_root, "%s", disc_root);
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

uint32_t recomp_doa3_roster_live(uint32_t guest)
{
    for (size_t i = 0; i < sizeof roster_relocations / sizeof roster_relocations[0]; ++i) {
        if (guest - roster_relocations[i][0] < roster_relocations[i][1]) {
            return roster_relocations[i][2] + (guest - roster_relocations[i][0]);
        }
    }
    return guest;
}

/* The lifted code reads and writes the save-block profile tables in the
   region; only the save write (0x00020c80) and load (0x00020de0) move the
   whole block, so they sync rows 0..18 with it. Rows 19+ go to the sidecar,
   only when roster.txt added characters. Format: "DOA3RP1\n", then each
   roster_save_rows table's bytes past the block's (profile rows 19..49, tag
   pair slots 171+) in order. An older file that ends at a table boundary
   (round 4's, before the tag tables) loads, and the tables it lacks are
   zeroed; a table cut short or trailing bytes fail the read.
   shortcut: the sidecar is written after the guest's save, not inside a
   save-journal operation (JP saves are not journaled yet); a crash between
   the two leaves it one save behind. */
#define SIDECAR_MAGIC "DOA3RP1\n"

/* to_block: region -> block; else block -> region. Returns false when the
   block still holds sub_00020DE0's 0xA5 fill (no save was loaded). */
static bool roster_save_copy(bool to_block)
{
    bool filled = true;

    for (size_t t = 0; t < sizeof roster_save_rows / sizeof roster_save_rows[0]; ++t) {
        const uint32_t n = roster_save_rows[t][1];
        uint8_t *block = recomp_memory(roster_save_rows[t][0], n);
        for (uint32_t i = 0u; i < n && filled; ++i) filled = block[i] == 0xA5u;
    }
    if (!to_block && filled) return false;
    for (size_t t = 0; t < sizeof roster_save_rows / sizeof roster_save_rows[0]; ++t) {
        const uint32_t n = roster_save_rows[t][1];
        uint8_t *block = recomp_memory(roster_save_rows[t][0], n);
        if (to_block) memcpy(block, roster_at(roster_save_rows[t][2]), n);
        else memcpy(roster_at(roster_save_rows[t][2]), block, n);
    }
    return true;
}

/* 1c7d35, 1c8f0e and 1c9278 reach the ranking tables through the save
   block's base (0x59D088 + offset in a register), which
   data_relocations.json cannot see, so they run on the block: the
   region's rows 0..18 are copied in, and every block byte the call changes
   is copied back (tables it reaches by their own address stay in the
   region and are left alone). */
static void block_call(void (*gen)(void))
{
    static uint8_t before[0x3000];
    size_t at = 0;

    roster_save_copy(true);
    for (size_t t = 0; t < sizeof roster_save_rows / sizeof roster_save_rows[0]; ++t) {
        const uint32_t n = roster_save_rows[t][1];
        assert(at + n <= sizeof before);
        memcpy(before + at, recomp_memory(roster_save_rows[t][0], n), n);
        at += n;
    }
    gen();
    at = 0;
    for (size_t t = 0; t < sizeof roster_save_rows / sizeof roster_save_rows[0]; ++t) {
        const uint32_t n = roster_save_rows[t][1];
        const uint8_t *block = recomp_memory(roster_save_rows[t][0], n);
        uint8_t *region = roster_at(roster_save_rows[t][2]);
        for (uint32_t i = 0u; i < n; ++i) if (block[i] != before[at + i]) region[i] = block[i];
        at += n;
    }
}

static void roster_sidecar(bool write)
{
    char path[1100], temp[1110];
    char magic[sizeof SIDECAR_MAGIC - 1u];
    FILE *file;
    bool ok, ended = false;

    snprintf(path, sizeof path, "%s/.recomp-storage/roster-profiles-v1", roster_root);
    snprintf(temp, sizeof temp, "%s.tmp", path);
    file = fopen(write ? temp : path, write ? "wb" : "rb");
    if (file == NULL) return;
    ok = write ? fwrite(SIDECAR_MAGIC, 1u, sizeof magic, file) == sizeof magic
               : fread(magic, 1u, sizeof magic, file) == sizeof magic && memcmp(magic, SIDECAR_MAGIC, sizeof magic) == 0;
    for (size_t t = 0; ok && t < sizeof roster_save_rows / sizeof roster_save_rows[0]; ++t) {
        uint8_t *rows = roster_at(roster_save_rows[t][2] + roster_save_rows[t][1]);
        const size_t n = roster_save_rows[t][3] - roster_save_rows[t][1];
        size_t got;

        if (write) { ok = fwrite(rows, 1u, n, file) == n; continue; }
        got = ended ? 0u : fread(rows, 1u, n, file);
        ended = got == 0u;
        if (ended) memset(rows, 0, n);
        else ok = got == n;
    }
    if (!write && ok) ok = fgetc(file) == EOF;
    if (fclose(file) != 0) ok = false;
    if (write && ok) {
        remove(path);  /* shortcut: rename does not replace on Windows */
        ok = rename(temp, path) == 0;
    }
    if (!ok) fprintf(stderr, "recomp roster: %s %s failed\n", write ? "writing" : "reading", path);
}

/* save write: copies the block to a buffer and writes T:\DOA3SAVE */
extern void sub_00020C80_gen(void);
void sub_00020C80(void)
{
    const bool writes = *recomp_memory(0x0059D0DAu, 1u) != 1u && *recomp_memory(0x00C59870u, 1u) != 1u;
    const unsigned session = rank_session;

    if (session != ~0u) rank_swap(session);   /* save the rows in place */
    roster_save_copy(true);
    sub_00020C80_gen();
    if (writes && roster_added) roster_sidecar(true);
    if (session != ~0u) rank_swap(session);
}

/* save load: copies T:\DOA3SAVE over the block only when its signature
   matches. The block's copies of the moved tables are dead storage, so a
   0xA5 fill there tells whether the load happened. */
extern void sub_00020DE0_gen(void);
void sub_00020DE0(void)
{
    for (size_t t = 0; t < sizeof roster_save_rows / sizeof roster_save_rows[0]; ++t) {
        const uint32_t n = roster_save_rows[t][1];
        memset(recomp_memory(roster_save_rows[t][0], n), 0xA5, n);
    }
    sub_00020DE0_gen();
    if (roster_save_copy(false) && roster_added) {
        roster_sidecar(false);
        rank_defaults(0u, 4u, true);
    }
}

/* Rankings (save block): per profile 3 entries; Time Attack 0x24 B (RK0
   single, RK1 tag), Survival 0x28 B (RK2, RK3). An entry holds the char byte
   (+0x12 / +0x15), the tag partner or 0xff, and the name (+0x14 / +0x18;
   Survival keeps the wins byte at +0x14).
   FUN_001c9278 resets the 19 stock profiles from 0x2ff190 / 0x2ff2f0 with
   names from 0x2ff610[profile]; rank_defaults does the same for added
   profiles (char = their own, partner and names = like's), all of them or
   only those whose rows are empty (a zero time would beat every run). */
static const struct { uint32_t table, stride, entry, source, at, name; bool tag; } rank_tables[] = {
    {SAVE_RK0, 0x6Cu, 0x24u, 0x002FF190u, 0x12u, 0x14u, false}, {SAVE_RK1, 0x6Cu, 0x24u, 0x002FF190u, 0x12u, 0x14u, true},
    {SAVE_RK2, 0x78u, 0x28u, 0x002FF2F0u, 0x15u, 0x18u, false}, {SAVE_RK3, 0x78u, 0x28u, 0x002FF2F0u, 0x15u, 0x18u, true},
};

static bool rank_empty(const uint8_t *row, uint32_t n)
{
    for (uint32_t i = 0u; i < n; ++i) if (row[i] != 0u) return false;
    return true;
}

/* first, count: rank_tables entries (0..1 Time Attack, 2..3 Survival) */
static void rank_defaults(size_t first, size_t count, bool only_empty)
{
    for (unsigned p = STOCK_PROFILES; p < ROSTER_PROFILES; ++p) {
        const unsigned like = profile_like[p];

        if (profile_char[p] == 0u) continue;
        for (size_t t = first; t < first + count; ++t) {
            uint8_t *row = roster_at(rank_tables[t].table + rank_tables[t].stride * p);

            if (only_empty && !rank_empty(row, rank_tables[t].stride)) continue;
            for (uint32_t c = 0u; c < 3u; ++c) {
                uint8_t *e = row + rank_tables[t].entry * c;
                const uint32_t names = *recomp_memory_u32(0x002FF610u + 4u * like);

                memcpy(e, recomp_memory(rank_tables[t].source + rank_tables[t].entry * c, rank_tables[t].entry),
                       rank_tables[t].entry);
                e[rank_tables[t].at] = profile_char[p];
                e[rank_tables[t].at + 1u] = rank_tables[t].tag ? *recomp_memory(0x002FF65Cu + like, 1u) : 0xFFu;
                snprintf((char *)e + rank_tables[t].name, rank_tables[t].entry - rank_tables[t].name, "%s",
                         (const char *)recomp_memory(*recomp_memory_u32(names + 4u * c), 1u));
            }
        }
    }
}

/* FUN_001c9278(table set): resets rankings (0 Time Attack, 1 Survival) or
   records (2) to the stock defaults; added profiles get theirs too. */
extern void sub_001C9278_gen(void);
void sub_001C9278(void)
{
    const uint8_t set = *recomp_memory(recomp_runtime.registers.esp + 4u, 1u);

    rank_session_end();
    block_call(sub_001C9278_gen);
    if (set < 2u) rank_defaults(2u * set, 2u, false);
}

/* FUN_000885e0 (Time Attack / Survival result) takes the ranks of
   FUN_001c8f0e(table): [0] total, [1] the char's own table as profile |
   rank << 8, [2] tag total, [3] the char's tag table. It inserts the run
   (1c7d35), asks the name (8b7f0 -> 8c350) and shows the tables only when
   a total rank is < 10 or a profile byte is < 0x13, and 1c7d35 / 8c350 skip
   a profile above 18 too. So for an added profile 1c8f0e's wrapper swaps
   its rows (and the per-profile records 1c7d35 reads) with RANK_PROXY's
   until 885e0 returns, and has 1c8f0e rank it as RANK_PROXY (its compact
   entry, 0x2ff868[char], set to RANK_PROXY for the call); the save wrapper
   swaps the rows back around the write. */
#define RANK_PROXY 0u
static unsigned rank_session = ~0u;   /* added profile swapped with RANK_PROXY */

static void rank_swap(unsigned p)
{
    static const uint32_t rows[][2] = {
        {SAVE_RK0, 0x6Cu}, {SAVE_RK1, 0x6Cu}, {SAVE_RK2, 0x78u}, {SAVE_RK3, 0x78u},
        {SAVE_REC0, 2u}, {SAVE_REC1, 2u}, {SAVE_REC2, 2u}, {SAVE_U1, 1u}, {SAVE_U2, 1u},
    };

    for (size_t t = 0; t < sizeof rows / sizeof rows[0]; ++t) {
        uint8_t *a = roster_at(rows[t][0] + rows[t][1] * p), *b = roster_at(rows[t][0] + rows[t][1] * RANK_PROXY);
        for (uint32_t i = 0u; i < rows[t][1]; ++i) { uint8_t s = a[i]; a[i] = b[i]; b[i] = s; }
    }
}

static void rank_session_end(void)
{
    if (rank_session != ~0u) rank_swap(rank_session);
    rank_session = ~0u;
}

/* (table) in [esp + 4], called for tables 0..3 in turn, ranks player
   0x598af2's char (0x588838 + 5 * player) */
extern void sub_001C8F0E_gen(void);
void sub_001C8F0E(void)
{
    const uint8_t table = *recomp_memory(recomp_runtime.registers.esp + 4u, 1u);
    const uint8_t ch = *recomp_memory(0x00588838u + 5u * *recomp_memory(0x00598AF2u, 1u), 1u);
    uint8_t *compact = roster_at(CHAR_COMPACT) + ch;
    const uint8_t own = *compact;

    if (table == 0u) rank_session_end();
    if (own >= STOCK_PROFILES && own < ROSTER_PROFILES && rank_session == ~0u) {
        rank_swap(own);
        rank_session = own;
    }
    if (rank_session == own) *compact = RANK_PROXY;
    block_call(sub_001C8F0E_gen);
    *compact = own;
}

extern void sub_001C7D35_gen(void);   /* (mode, ranks): inserts the run */
void sub_001C7D35(void) { block_call(sub_001C7D35_gen); }

extern void sub_000885E0_gen(void);
void sub_000885E0(void) { sub_000885E0_gen(); rank_session_end(); }

/* FUN_000653e0 / FUN_00065ec0 (records screens, Time Attack / Survival):
   the per-character list walks a ranking table's rows from its base to the
   next table's base, skipping profiles 3 (unless Ein is unlocked), 12 and 16,
   into a 19-slot stack array. In the region that walk would cover 50+ rows,
   so data_relocations.json keeps those constants (start, skips, end) at the
   save block, and the block gets the region's rows 0..18 first. Added
   profiles join the list in sub_00064670. */
extern void sub_000653E0_gen(void);
extern void sub_00065EC0_gen(void);
void sub_000653E0(void) { roster_save_copy(true); sub_000653E0_gen(); }
void sub_00065EC0(void) { roster_save_copy(true); sub_00065EC0_gen(); }

/* FUN_00064670(list, ..., count): both records screens call it right after
   sorting their per-character list (list 1 single, 3 tag), the array and its
   count still in the caller's frame: 653e0 (returns to 0x655b5) keeps them at
   its [esp + 0x64] / [esp + 0x4c] and the count in EBP, sorted by time up;
   65ec0 (0x660cb) at [esp + 0x5c] / [esp + 0x14], by score then wins down.
   Added profiles' rows go into the free slots in that order, and the count
   (caller's local, the pushed argument, EBP) grows to match.
   shortcut: at most 19 rows in all (16 or 17 stock), so only the first 2 or
   3 added profiles are listed. */
extern void sub_00064670_gen(void);
void sub_00064670(void)
{
    const uint32_t esp = recomp_runtime.registers.esp, ret = *recomp_memory_u32(esp);
    const uint32_t list = *recomp_memory_u32(esp + 4u);
    const bool time = ret == 0x000655B5u;

    if (roster_added && (time || ret == 0x000660CBu) && (list == 1u || list == 3u)) {
        const uint32_t array = esp + 0x14u + (time ? 0x64u : 0x5Cu), count = esp + 0x14u + (time ? 0x4Cu : 0x14u);
        const size_t table = (time ? 0u : 2u) + (list == 3u);
        uint32_t n = *recomp_memory_u32(count);

        for (unsigned p = STOCK_PROFILES; p < ROSTER_PROFILES && n < 19u; ++p) {
            const uint32_t row = rank_tables[table].table + rank_tables[table].stride * p;
            uint32_t i = n++;

            if (profile_char[p] == 0u) { --n; continue; }
            for (; i > 0u; --i) {
                const uint32_t prev = *recomp_memory_u32(array + 4u * (i - 1u));
                const uint32_t a = *recomp_memory_u32(row + 8u), b = *recomp_memory_u32(prev + 8u);
                const bool before = time ? a < b
                    : a > b || (a == b && *recomp_memory(row + 0x14u, 1u) > *recomp_memory(prev + 0x14u, 1u));
                if (!before) break;
                *recomp_memory_u32(array + 4u * i) = prev;
            }
            *recomp_memory_u32(array + 4u * i) = row;
        }
        *recomp_memory_u32(count) = n;
        *recomp_memory_u32(esp + 0x10u) = n;
        if (time) recomp_runtime.registers.ebp = n;
    }
    sub_00064670_gen();
}

/* FUN_000a47e0 (movie start), movie index in EAX: plays 0x2d4dd8[index],
   movie 0 (the opening) when index > 18. An added profile's ending (index =
   profile, see PROF_ENDING) plays through index 1's slot, its name swapped
   in for the call; index 0 would take the opening's audio path. */
#define MOVIE_PROXY 1u
extern void sub_000A47E0_gen(void);
void sub_000A47E0(void)
{
    const uint32_t index = recomp_runtime.registers.eax;
    uint32_t name, proxy;

    if (index < STOCK_MOVIES || index >= ROSTER_PROFILES ||
        (name = *recomp_memory_u32(MOVIE_NAMES + 4u * index)) == 0u) {
        sub_000A47E0_gen();
        return;
    }
    proxy = *recomp_memory_u32(MOVIE_NAMES + 4u * MOVIE_PROXY);
    *recomp_memory_u32(MOVIE_NAMES + 4u * MOVIE_PROXY) = name;
    recomp_runtime.registers.eax = MOVIE_PROXY;
    sub_000A47E0_gen();
    *recomp_memory_u32(MOVIE_NAMES + 4u * MOVIE_PROXY) = proxy;
}


static uint8_t grid_char(unsigned record)
{
    return roster_at(GRID_RECORDS)[10u * record];
}

/* Each player's select state (4 of 0x38 bytes from 0x9b79d0, the select
   screen 0x9b79c0 + 0x10) keeps a byte per grid cell (the costume last
   shown there) at state + cell: room for 18 cells, then the state's own
   fields (+0x12 pad, +0x14 char, +0x16 cell).
   The bodies that index a state by a cell (ccb80, charsel_confirm cc760,
   the taken-cell check ccd90 and team mode's store of the taken cell's
   costume in cb370 and cb760) run on a cell c of 18 or more inside
   grid_view(c): cells c and GRID_PROXY_CELL swapped for all four states
   (slots, each state's byte at +17 against grid_cell_bytes, cell fields).
   The swap is its own inverse, so a second grid_view(c) restores it. */
#define CHARSEL_STATES 0x009B79D0u
#define CHARSEL_STATE_SIZE 0x38u
#define GAME_MODE 0x00596A33u
#define MODE_TEAM 0x0Au
#define GRID_CELL_BYTES 18u
#define GRID_PROXY_CELL 17u
static uint8_t grid_cell_bytes[4][256];
static bool grid_restoring;      /* inside sub_000CAAA0 or sub_000CADD0 */
static unsigned grid_full_count; /* the real count while it runs */

static bool grid_viewed(unsigned cell)
{
    return cell >= GRID_CELL_BYTES && cell < *recomp_memory_u32(GRID_COUNT);
}

static void grid_view(unsigned cell)
{
    uint8_t *slots = roster_at(GRID_SLOTS), swap = slots[GRID_PROXY_CELL];

    slots[GRID_PROXY_CELL] = slots[cell];
    slots[cell] = swap;
    for (unsigned p = 0u; p < 4u; ++p) {
        const uint32_t state = CHARSEL_STATES + CHARSEL_STATE_SIZE * p;
        uint8_t *own = recomp_memory(state + GRID_PROXY_CELL, 1u), *at = recomp_memory(state + 0x16u, 1u);

        swap = *own;
        *own = grid_cell_bytes[p][cell];
        grid_cell_bytes[p][cell] = swap;
        if (*at == cell) *at = GRID_PROXY_CELL;
        else if (*at == GRID_PROXY_CELL) *at = (uint8_t)cell;
    }
}

/* gen on the state's cell, in view when it is 18 or more */
static void grid_view_call(void (*gen)(void), uint32_t state)
{
    const unsigned cell = *recomp_memory(state + 0x16u, 1u);

    if (!grid_viewed(cell)) { gen(); return; }
    if (state - CHARSEL_STATES >= 4u * CHARSEL_STATE_SIZE) {
        fprintf(stderr, "recomp roster: select state 0x%08" PRIx32 " is not one of 0x%08x's 4\n", state, CHARSEL_STATES);
        gen();
        return;
    }
    grid_view(cell);
    gen();
    grid_view(cell);
}

extern void sub_000CCB80_gen(void);   /* costume of the state's cell, state in EAX */
void sub_000CCB80(void) { grid_view_call(sub_000CCB80_gen, recomp_runtime.registers.eax); }
extern void sub_000CC760_gen(void);   /* charsel_confirm(screen), player in AL */
void sub_000CC760(void)
{
    grid_view_call(sub_000CC760_gen, *recomp_memory_u32(recomp_runtime.registers.esp + 4u) + 0x10u +
                    CHARSEL_STATE_SIZE * (recomp_runtime.registers.eax & 0xFFu));
}
/* FUN_000cb760 (pad up/down on the cell, state at [esp + 8]): in team mode a
   taken cell copies the taker's costume to state + cell (0xcb7c1). */
extern void sub_000CB760_gen(void);
void sub_000CB760(void) { grid_view_call(sub_000CB760_gen, *recomp_memory_u32(recomp_runtime.registers.esp + 8u)); }

/* Calls a lifted function with one stack argument under a zero return
   address; the caller sets the registers it takes and keeps the rest. */
static void guest_call(void (*fn)(void), uint32_t arg)
{
    const uint32_t esp = recomp_runtime.registers.esp;

    recomp_runtime.registers.esp = esp - 8u;
    *recomp_memory_u32(esp - 8u) = 0u;
    *recomp_memory_u32(esp - 4u) = arg;
    fn();
    recomp_runtime.registers.esp = esp;
}

/* ccd90 as cb370 calls it (0xcb478): player ccef0(state) in CL, the cell at
   [esp + 4], the taken costume to [EDI]. True when the cell is taken (the tag
   partner's cell, or a char the side already has). */
extern void sub_000CCEF0(void);
extern void sub_000CCD90(void);
static bool grid_taken(uint32_t state, unsigned cell)
{
    const RecompRegisters saved = recomp_runtime.registers;
    const uint32_t frame = saved.esp - 0x10u;   /* the out byte at frame + 8 */
    const bool viewed = grid_viewed(cell);
    bool taken;

    if (viewed) grid_view(cell);
    recomp_runtime.registers.esp = frame;
    recomp_runtime.registers.eax = state;
    guest_call(sub_000CCEF0, 0u);
    recomp_runtime.registers.ecx = recomp_runtime.registers.eax & 0xFFu;
    recomp_runtime.registers.edi = frame + 8u;
    guest_call(sub_000CCD90, viewed ? GRID_PROXY_CELL : cell);
    taken = recomp_runtime.registers.eax != 0u;
    recomp_runtime.registers = saved;
    if (viewed) grid_view(cell);
    return taken;
}

/* A shuffle bag {u32 bytes, u8 size, u8 next, u8 shuffles}: the next byte,
   reshuffled by 0xd5eb0 (bag in ESI, shuffles on the stack) when used up. */
extern void sub_000D5EB0(void);
static uint8_t bag_draw(uint32_t bag)
{
    uint8_t *next = recomp_memory(bag + 5u, 1u);

    if (*next >= *recomp_memory(bag + 4u, 1u)) {
        recomp_runtime.registers.esi = bag;
        guest_call(sub_000D5EB0, *recomp_memory(bag + 6u, 1u));
    }
    return *recomp_memory(*recomp_memory_u32(bag) + (*next)++, 1u);
}

/* FUN_000cc980(side, controller): fills the side's team members (count at
   0x9b7c30, 5 slots a side) that are empty, or flagged at 0x5a28f0 in modes
   2-5, 9 and 10, with random grid cells: a cell from the bag at 0x404990
   until it holds a char that is not RANDOM, not char 3 when controller (the
   side's byte at 0x59cf58) is 0, and not on the side yet (cce40; team mode
   takes that member's costume instead); then a costume from the bag at
   0x404998 below the char's count, one up when the other side has the same
   char and costume (ccd40). The body counts no costumes for a char above 24
   and divides by that count (0xccad7), so this is the body in C with every
   char's count. */
#define TEAM_COUNT 0x009B7C30u     /* s8 per side */
#define TEAM_COSTUME 0x009B7C00u   /* 5 per side, 0xff = empty */
#define TEAM_CHAR 0x00588838u      /* 5 per side */
extern void sub_000CCE40(void);
extern void sub_000CCD40(void);
extern void sub_000CD870(void);
void sub_000CC980(void)
{
    const RecompRegisters saved = recomp_runtime.registers;
    const uint32_t side = *recomp_memory_u32(saved.esp + 4u), controller = *recomp_memory_u32(saved.esp + 8u);
    const uint8_t mode = *recomp_memory(GAME_MODE, 1u);
    const bool refill = (mode >= 2u && mode <= 5u) || mode == 9u || mode == MODE_TEAM;
    const uint32_t out = saved.esp - 8u;   /* cce40's costume byte, above the calls' frames */

    recomp_runtime.registers.esp = saved.esp - 0x10u;
    for (uint32_t i = 0u; (int32_t)i < (int8_t)*recomp_memory(TEAM_COUNT + side, 1u); ++i) {
        const uint32_t slot = 5u * side + i;
        uint8_t ch, costume, variant;

        if (*recomp_memory(TEAM_CHAR + slot, 1u) != CHAR_RANDOM &&
            (*recomp_memory(0x005A28F0u + slot, 1u) == 0u || !refill)) continue;
        for (;;) {
            unsigned costumes;

            ch = grid_char(roster_at(GRID_SLOTS)[bag_draw(0x00404990u) % *recomp_memory_u32(GRID_COUNT)]);
            if (ch == CHAR_RANDOM || ch == 0xFEu || (controller == 0u && ch == 3u)) continue;
            recomp_runtime.registers.ecx = side & 0xFFu;
            recomp_runtime.registers.edx = ch;
            guest_call(sub_000CCE40, out);
            if (recomp_runtime.registers.eax != 0u) {
                if (mode != MODE_TEAM) continue;
                costume = *recomp_memory(out, 1u);
                break;
            }
            costumes = roster_at(SAVE_TIER)[roster_at(CHAR_COMPACT)[ch]];
            if (costumes > roster_at(PICK_CAP)[ch]) costumes = roster_at(PICK_CAP)[ch];
            if (costumes == 0u) costumes = 1u;   /* the stock divide would fault */
            costume = (uint8_t)(bag_draw(0x00404998u) % costumes);
            recomp_runtime.registers.ecx = (1u - side) & 0xFFu;
            recomp_runtime.registers.edx = ch;
            recomp_runtime.registers.ebx = costume;
            guest_call(sub_000CCD40, 0u);
            if (recomp_runtime.registers.eax != 0u) costume = (uint8_t)((costume + 1u) % costumes);
            break;
        }
        *recomp_memory(TEAM_COSTUME + slot, 1u) = costume;
        recomp_runtime.registers.eax = ch;
        guest_call(sub_000CD870, 0u);
        variant = (uint8_t)recomp_runtime.registers.eax;
        *recomp_memory(TEAM_CHAR + slot, 1u) = ch;
        *recomp_memory(0x005A26F8u + slot, 1u) = variant;
        *recomp_memory(0x0059CF44u + slot, 1u) = ch;
        *recomp_memory(0x005A2620u + slot, 1u) = variant;
        if (mode == MODE_TEAM) {
            *recomp_memory(0x00599881u + 16u * side + i, 1u) = ch;
            *recomp_memory(0x00599886u + 16u * side + i, 1u) = variant;
        }
    }
    recomp_runtime.registers = saved;
    recomp_runtime.registers.esp += 4u; /* ret */
}

/* FUN_000caaa0 (first select entry, from 0xd6248) and FUN_000cadd0 (select
   screen entry), screen at [esp + 4]: build the grid, then put each player's
   cursor back on its last char (RANDOM and chars above 24 on the last cell,
   others on CHAR_CELL) and write the costume to state + cell. Past 18 cells
   they run on a count of 18 (charsel_build_grid caps it), so that byte stays
   in range; then RANDOM's cursors move to the real last cell and an added
   char's to its own cell. */
static void grid_restore_call(void (*gen)(void))
{
    const uint32_t states = *recomp_memory_u32(recomp_runtime.registers.esp + 4u) + 0x10u;

    grid_full_count = 0u;
    grid_restoring = true;
    gen();
    grid_restoring = false;
    if (grid_full_count == 0u) return;
    *recomp_memory_u32(GRID_COUNT) = grid_full_count;
    for (unsigned p = 0u; p < 4u; ++p) {
        const uint32_t state = states + CHARSEL_STATE_SIZE * p;
        const uint8_t ch = *recomp_memory(state + 0x14u, 1u);
        uint8_t *cell = recomp_memory(state + 0x16u, 1u);

        if (ch == CHAR_RANDOM) *cell = (uint8_t)(grid_full_count - 1u);
        else if (ch >= STOCK_CHARS && ch < ROSTER_CHARS && roster_at(CHAR_CELL)[ch] != 0u) *cell = roster_at(CHAR_CELL)[ch];
    }
}
extern void sub_000CAAA0_gen(void);
extern void sub_000CADD0_gen(void);
void sub_000CAAA0(void) { grid_restore_call(sub_000CAAA0_gen); }
void sub_000CADD0(void) { grid_restore_call(sub_000CADD0_gen); }

/* charsel_build_grid (0x000CD9E0), mode in EAX: the stock body builds the
   16-cell grid (Ein locked, or mode 0) or the 18-cell grid, the char -> cell
   map and the costume-unlock merge. With added records, rebuild the cells as
   every fighter record in order (Ein only when unlocked), then RANDOM, padded
   with a second RANDOM to an even count: the cursor and draw code lay the grid
   out as two rows of count / 2, and RANDOM resolves to the last cell. Past 20
   cells grid_layout takes over the rows (see sub_000CB370).
   shortcut: stock code resets only 18 cell offsets (0x9b7880) when the grid
   slides out, so cells 18+ keep their last offset. */
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
    memset(grid_cell_bytes, 0, sizeof grid_cell_bytes);
    if (grid_restoring && count > GRID_CELL_BYTES) {
        grid_full_count = count;
        *recomp_memory_u32(GRID_COUNT) = GRID_CELL_BYTES;
    }
}

/* Past GRID_TWO_ROWS cells: R rows scaled by 2 / R into the stock two rows'
   band, at most 5R cells a row (the stock 10 at full size), rows filled
   evenly and the last one centred. The stock draw places cell i at column
   i % (count / 2), row i / (count / 2), 68 x 54 px apart (60 x 48 cells)
   from y 0x13f, centred on the frame width (0x596a88), plus the cell's
   slide offset 0x9b7880[i]. */
struct grid_shape { unsigned rows, cols; float scale; };

static struct grid_shape grid_shape(unsigned count)
{
    unsigned rows = 2u;

    while (count > 5u * rows * rows) ++rows;
    return (struct grid_shape){rows, (count + rows - 1u) / rows, 2.0f / (float)rows};
}

static unsigned grid_row_length(struct grid_shape s, unsigned count, unsigned row)
{
    return row + 1u == s.rows ? count - row * s.cols : s.cols;
}

static bool grid_layout(unsigned cell, int *x, int *y, float *scale)
{
    const unsigned count = *recomp_memory_u32(GRID_COUNT);
    struct grid_shape s;
    unsigned row, col;
    float pitch_x, pitch_y;
    int16_t offset;

    if (count <= GRID_TWO_ROWS || cell >= count) return false;
    s = grid_shape(count);
    row = cell / s.cols;
    col = cell % s.cols;
    pitch_x = 0x44 * s.scale;
    pitch_y = 0x36 * s.scale;
    memcpy(&offset, roster_at(GRID_OFFSETS + 2u * cell), 2u);
    *x = (int)((*recomp_memory_u16(0x00596A88u) - s.cols * pitch_x + 8.0f) / 2.0f +
               (s.cols - grid_row_length(s, count, row)) * pitch_x / 2.0f + col * pitch_x) + offset;
    *y = (int)(0x13F + row * pitch_y);
    *scale = s.scale;
    return true;
}

/* x and y packed as the sprite calls take them, (y << 12 | x) in 24 bits */
static uint32_t grid_pack(uint32_t packed, int x, int y)
{
    return (packed & 0xFF000000u) | ((uint32_t)y & 0xFFFu) << 12 | ((uint32_t)x & 0xFFFu);
}

/* The cell a direction press reaches (pad bits 1 up, 2 down, 4 left, 8
   right; right and down win, as in the stock code): horizontal moves wrap in
   the row, vertical ones wrap over the rows and keep the nearest column. */
static unsigned grid_step(unsigned cell, unsigned bits, unsigned count)
{
    const struct grid_shape s = grid_shape(count);
    unsigned row = cell / s.cols, col = cell % s.cols, length = grid_row_length(s, count, row);

    if (bits & 0xCu) col = (col + length + ((bits & 8u) ? 1u : length - 1u)) % length;
    if (bits & 0x3u) {
        const int half = (int)(s.cols - length) + 2 * (int)col;   /* x in half cells */
        int next;

        row = (row + ((bits & 2u) ? 1u : s.rows - 1u)) % s.rows;
        length = grid_row_length(s, count, row);
        next = (half - (int)(s.cols - length)) / 2;
        col = next < 0 ? 0u : (unsigned)next >= length ? length - 1u : (unsigned)next;
    }
    return row * s.cols + col;
}

/* charsel_cursor_move (0x000CB370), the player's select state at [esp + 8]
   (+0x12 pad, +0x16 cell): moves the cell by the pad's direction bits
   (u16 at 0x9b7c10 + pad * 8) on the two-row layout, then handles a changed
   cell (preview, sound, taken cells). Past 20 cells the wrapper picks the
   target on grid_layout's rows and hands the body a one-cell right (or left)
   move that lands on it, so the rest of the body runs as stock.
   A taken target (tag partner's cell) gets the stock skip here, one more step
   (down for a vertical move); team mode keeps it and copies its costume. A
   target of 18 or more runs the body in grid_view as GRID_PROXY_CELL. */
extern void sub_000CB370_gen(void);
void sub_000CB370(void)
{
    const uint32_t state = *recomp_memory_u32(recomp_runtime.registers.esp + 8u);
    const unsigned count = *recomp_memory_u32(GRID_COUNT);
    uint8_t *cell = recomp_memory(state + 0x16u, 1u);
    const uint32_t input = 0x009B7C10u + 8u * (uint32_t)(int32_t)(int8_t)*recomp_memory(state + 0x12u, 1u);
    const uint16_t bits = *recomp_memory_u16(input);
    unsigned target;

    if (count <= GRID_TWO_ROWS || (bits & 0xFu) == 0u || *cell >= count) { sub_000CB370_gen(); return; }
    target = grid_step(*cell, bits, count);
    if (target != *cell && *recomp_memory(GAME_MODE, 1u) != MODE_TEAM && grid_taken(state, target))
        target = grid_step(target, (bits & 0xCu) ? bits & 0xCu : 2u, count);
    *recomp_memory_u16(input) = (uint16_t)(bits & ~0xFu);
    if (target == *cell) {
        sub_000CB370_gen();
    } else {
        const unsigned shown = grid_viewed(target) ? GRID_PROXY_CELL : target;
        const bool right = shown % (count / 2u) != 0u;

        if (shown != target) grid_view(target);
        *cell = (uint8_t)(right ? shown - 1u : shown + 1u);
        *recomp_memory_u16(input) |= right ? 8u : 4u;
        sub_000CB370_gen();
        if (shown != target) grid_view(target);
    }
    *recomp_memory_u16(input) = bits;
}

/* Past 20 cells, the grid draws place and scale each sprite by grid_layout:
   0x000CF8E0 draws the cells (cell index in EBX at its 0x6df00 calls, in EDI
   at its 0x6e5a0 calls) and 0x000D1730 the players' cursors (0x6df70, the
   player's cell at [EDI - 0x1e], 10 px up and left of the cell). Their sprite
   records {u32 sprite, u32 packed x, y} are in EAX, scaled by the floats at
   0x5b12d8 (x) and 0x5b12dc (y); 0x6e5a0 draws the box record in ESI
   (+4 u16 width, +6 height) at x, y from 0x5b12e8. */
static unsigned grid_drawing;  /* 1 cells, 2 cursors */

extern void sub_000CF8E0_gen(void);
void sub_000CF8E0(void) { grid_drawing = 1u; sub_000CF8E0_gen(); grid_drawing = 0u; }
extern void sub_000D1730_gen(void);
void sub_000D1730(void) { grid_drawing = 2u; sub_000D1730_gen(); grid_drawing = 0u; }

static void grid_scale_f32(uint32_t guest, float scale)
{
    float value;

    memcpy(&value, recomp_memory(guest, 4u), 4u);
    value *= scale;
    memcpy(recomp_memory(guest, 4u), &value, 4u);
}

static void grid_draw(void (*gen)(void), unsigned when, uint32_t cell, uint32_t record, int inset)
{
    const unsigned drawing = grid_drawing;
    uint8_t saved[8];
    uint16_t *box = NULL;
    float scale = 1.0f;
    int x, y;

    grid_drawing = 0u;
    if (drawing == when && grid_layout(cell, &x, &y, &scale)) {
        inset = (int)(inset * scale);
        if (record != 0u) {
            *recomp_memory_u32(record + 4u) = grid_pack(*recomp_memory_u32(record + 4u), x - inset, y - inset);
            memcpy(saved, recomp_memory(0x005B12D8u, 8u), 8u);
            grid_scale_f32(0x005B12D8u, scale);
            grid_scale_f32(0x005B12DCu, scale);
        } else {
            *recomp_memory_u16(0x005B12E8u) = (uint16_t)x;
            *recomp_memory_u16(0x005B12EAu) = (uint16_t)y;
            box = recomp_memory_u16(recomp_runtime.registers.esi + 4u);
            memcpy(saved, box, 4u);
            box[0] = (uint16_t)(box[0] * scale);
            box[1] = (uint16_t)(box[1] * scale);
        }
    }
    gen();
    if (scale != 1.0f) memcpy(box != NULL ? (uint8_t *)box : recomp_memory(0x005B12D8u, 8u), saved, box != NULL ? 4u : 8u);
    grid_drawing = drawing;
}

extern void sub_0006DF00_gen(void);
void sub_0006DF00(void)
{
    grid_draw(sub_0006DF00_gen, 1u, recomp_runtime.registers.ebx, recomp_runtime.registers.eax, 0);
}
extern void sub_0006E5A0_gen(void);
void sub_0006E5A0(void) { grid_draw(sub_0006E5A0_gen, 1u, recomp_runtime.registers.edi, 0u, 0); }
extern void sub_0006DF70_gen(void);
void sub_0006DF70(void)
{
    const uint32_t cell = grid_drawing == 2u
        ? (uint32_t)(int32_t)(int8_t)*recomp_memory(recomp_runtime.registers.edi - 0x1Eu, 1u) : 0u;
    grid_draw(sub_0006DF70_gen, 2u, cell, recomp_runtime.registers.eax, 10);
}

/* A tag pair with an added char -> its slot among the added pairs, or ~0u
   for a stock pair. */
static unsigned tag_added_slot(unsigned a, unsigned b)
{
    const unsigned high = a > b ? a : b, low = a > b ? b : a;

    return high >= STOCK_CHARS && high < ROSTER_CHARS ? (high - STOCK_CHARS) * ROSTER_CHARS + low : ~0u;
}

/* FUN_0007a330: the tag-team pair of chars AL and DL -> its slot in the
   match counts (SAVE_TAG; 0x20990, 0x7a2f0 and 0x79bd0 count a tag match
   there). The body searches the stock pair list 0x2ff898 from
   0x2ff9a8[lower char], which covers chars 0..24, so a pair with an added
   char got another pair's slot, or one past the 171. Such pairs take
   171 + tag_added_slot instead.
   shortcut: 0x8ca70 (a char's tag total) and 0x67670 (records screen) walk
   only the 136 stock pairs. */
extern void sub_0007A330_gen(void);
void sub_0007A330(void)
{
    const unsigned slot = tag_added_slot(recomp_runtime.registers.eax & 0xFFu, recomp_runtime.registers.edx & 0xFFu);

    if (slot == ~0u) { sub_0007A330_gen(); return; }
    recomp_runtime.registers.eax = TAG_STOCK_SLOTS + slot;
    recomp_runtime.registers.esp += 4u; /* ret */
}

/* FUN_00085bf0 (profiles in CL and DL, load slot in BL): loads the tag pair
   file PROF_PAIRS[a * 19 + b] to the slot's buffer + PROF_PAIR_DEST[a]. An
   added profile's row is like's row with the stock 19 columns, so an added
   column profile b runs as like's profile (else it reads the next row's
   entry: another pair's file, which the loader then misreads). */
extern void sub_00085BF0_gen(void);
void sub_00085BF0(void)
{
    const unsigned b = recomp_runtime.registers.edx & 0xFFu;

    if (b >= STOCK_PROFILES && b < ROSTER_PROFILES)
        recomp_runtime.registers.edx = (recomp_runtime.registers.edx & ~0xFFu) | profile_like[b];
    sub_00085BF0_gen();
}

/* FUN_00020bd0 (match end, from 0x79bd0): in a tag match (mode 9 or the
   tag flag 0xc59866) it counts the team of side 0x5a25fe (chars at
   0x59cf44 + 5 * side) at 0x59fb4e + 2 * (pair slot & 0xff), a byte index,
   so an added pair's slot would wrap onto another pair. Count those in
   SAVE_TAG_WINS; everything else runs the body. */
extern void sub_00020BD0_gen(void);
void sub_00020BD0(void)
{
    const uint8_t side = *recomp_memory(0x005A25FEu, 1u);
    const bool tag = *recomp_memory(0x005A2858u, 1u) == 9u || *recomp_memory(0x00C59866u, 1u) == 1u;
    const uint32_t team = 0x0059CF44u + (side != 0u ? 5u : 0u);
    const unsigned slot = tag_added_slot(*recomp_memory(team, 1u), *recomp_memory(team + 1u, 1u));
    uint16_t count;

    if (side == 2u || !tag || slot == ~0u) { sub_00020BD0_gen(); return; }
    memcpy(&count, roster_at(SAVE_TAG_WINS + 2u * slot), 2u);
    ++count;
    memcpy(roster_at(SAVE_TAG_WINS + 2u * slot), &count, 2u);
    recomp_runtime.registers.esp += 4u; /* ret */
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

/* FUN_00085e40 (load_queue_run's per-fighter life-bar name texture, char in
   the byte at [esp + 8]): a char with a record in 0x3092f4 loads its
   0x404b1c file, a 64x16 or 128x16 P8 XPR, and registers it (FUN_00071f20).
   Ids above 24 and ids without a record take a path no stock fighter
   reaches: it loads loadfile entry <char> to guest address <char> (entry 25
   overwrote code) and then a file named by uninitialised stack words
   (FUN_00085f70). Run it as the added character's name_strip= instead, a
   stock fighter with a record. With name_strip=ENTRY it runs as char 0 with
   that record's entry (u16 at 0x404b1c + 0x40 * record) set to ENTRY for
   the call: the record's other fields (texture slot, count 2) are written
   per call and the file goes to the player's own buffer, so sharing the
   record with a char 0 opponent is safe. */
extern void sub_00085E40_gen(void);
void sub_00085E40(void)
{
    uint8_t *arg = recomp_memory(recomp_runtime.registers.esp + 8u, 1u);
    const uint8_t id = *arg;
    uint8_t *entry = NULL, old[2];

    if (id >= STOCK_CHARS && id < ROSTER_CHARS && roster_name_file[id] != 0u) {
        const uint8_t record = *recomp_memory(*recomp_memory_u32(0x003092F4u), 1u);  /* char 0's */
        entry = recomp_memory(0x00404B1Cu + 0x40u * record, 2u);
        memcpy(old, entry, 2u);
        memcpy(entry, &roster_name_file[id], 2u);
        *arg = 0u;
    } else if (id >= STOCK_CHARS && id < ROSTER_CHARS) {
        *arg = roster_name_strip[id];
    }
    sub_00085E40_gen();
    if (entry != NULL) memcpy(entry, old, 2u);
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
