# Japan 3.1 boot test

Time-boxed test (two sessions) of lifting the Japan 3.1 XBE (XDK 4134) on the
USA runtime. The user supplies only the Japan disc.

## Recipe

- `tools/doa3-jp/manual_functions.json` and `seed_functions.json`: the USA
  recipe moved to Japan addresses with Ghidra Version Tracking and BSim
  (session `DOA3-US-to-JP`). All 83 entries mapped; 6 rest on two kinds of
  evidence instead of an exact match.
- `recomp-runtime/program_forwards_jp.c`: each Japan entry point forwards the
  USA address it replaces, so USA-keyed adapters bind unchanged. Select it with
  `-DRECOMP_PROGRAM_FORWARDS=program_forwards_jp.c`.
- Japan calls `D3DDevice_Swap` (0x001E14D0, `ret 4`) where the USA build calls
  Present; `d3d_frame_adapter.c` binds it to its own adapter.
- XDK 4134 changed `IDirectSoundBuffer_SetMixBins` and `SetMixBinVolumes` to
  take `LPCDSMIXBINS`; their adapters still expect the 3936 arguments.

The lift translates 6584 of 6664 functions with no failures. Hand-written code
still calls about 38 USA guest functions directly; the build links them as
fail-loud stubs.

## Session 1 result

Boots through kernel setup, save folders, the `Z:` cache format and
`XInitDevices`, then stalls in `Direct3D_CreateDevice` without a frame.
D3D8 4134 rearranged `CDevice`, so `d3d_miniport_adapter.c` writes the wrong
fields. Offsets read from both `CDevice::KickOff` bodies:

| Field | USA 3925 | Japan 4134 |
|---|---|---|
| flags | 0x000C | 0x0008 |
| kicked put | 0x0018 | 0x002C |
| fence | 0x001C | 0x0030 |
| fence context | 0x03F0 | 0x0034 |
| alternate put | 0x0400 | 0x035C |
| channel | 0x2304 | 0x2264 |
| flips done | 0x2518 | 0x2458 |
| frame counter | 0x2B60 | 0x2ABC |

The fence record table offset (USA 0x00E4) is not yet found for Japan.

## Session 2

Make the miniport layout per build, find the fence records, and continue to
the next stop. Decide at the end: Japan as the main target, or USA with the
3.1 balance table.