# Japan 3.1 boot test

Time-boxed test (two sessions) of lifting the Japan 3.1 XBE (XDK 4134) on the
USA runtime. The user supplies only the Japan disc.

## Build

Configure the runner with `-DRECOMP_DOA3_JP=ON`. It compiles
`program_forwards_jp.c` and defines `RECOMP_DOA3_JP_BINDINGS`.

- `tools/doa3-jp/manual_functions.json` and `seed_functions.json`: the USA
  recipe moved to Japan addresses with Ghidra Version Tracking and BSim
  (session `DOA3-US-to-JP`).
- `program_forwards_jp.c` holds the Japan stubs and a Japan-to-USA table.
  `recomp_lookup_manual` translates through it, so USA-keyed adapters bind
  unchanged. Addresses outside the table get no adapter: three Japan functions
  sit at USA adapter keys (0x00021930, 0x000E8100, 0x001B8890).
- `doa3_build.h`: `DOA3(usa, japan)` picks a guest address for the lifted XBE.
- Japan calls `D3DDevice_Swap` (0x001E14D0, `ret 4`) where the USA build calls
  Present; `d3d_frame_adapter.c` binds it to its own adapter.
- DSOUND 4134 `SetMixBinVolumes` takes `LPCDSMIXBINS` (`ret 8`); the trace-only
  adapter pops two arguments on Japan. `SetMixBins` keeps `ret 8`.

The Japan recipe binds 61 of the 83 USA hooks. The 17 D3D state and draw
hooks (`SetTexture`, `SetStreamSource`, `SetIndices`, `SetVertexShader`, the
render-state setters and the three draw calls) and the 5 game seams
(collision passes, action category, auto-save) stay lifted: their adapters
model D3D8 3925 and USA game data. Japan draws nothing until they are ported.

## D3D8 4134 layout

`CDevice` fields, read from `KickOff`, `D3D_SetFence`, `GetBackBuffer`,
`InitializeFrameBuffers`, `SetTile` and the render-state setters:

| Field | USA 3925 | Japan 4134 |
|---|---|---|
| flags | 0x000C | 0x0008 |
| kicked put | 0x0018 | 0x002C |
| fence | 0x001C | 0x0030 |
| fence context | 0x03F0 | 0x0034 |
| fence records | 0x00E4 | 0x0050 |
| alternate put | 0x0400 | 0x035C |
| pixel shader | 0x0414 | 0x0370 |
| render target | 0x040C | 0x2070 |
| depth stencil | 0x0410 | 0x2074 |
| back buffers | embedded at 0x2150 | pointer table at 0x207C, surfaces at 0x208C |
| auto depth surface | 0x219C | 0x20D4 |
| back-buffer size | 0x21B4, 0x21B8 | only in the surface `Size` word |
| tiles | 0x21BC | 0x211C |
| channel | 0x2304 | 0x2264 |
| flips done | 0x2518 | 0x2458 |
| frame counter | 0x2B60 | 0x2ABC |

Globals: `g_pDevice` 0x001C3390 to 0x001EDE80, static device 0x001C0800 to
0x001EB3A0. The dirty mask left the device: USA 0x001C0808 (device + 8) is
Japan 0x001EAD68, where device + 8 holds the flags. The 3925 adapters that OR
dirty bits into device + 8 would corrupt Japan's flags.

## Address map

A diff of matched function pairs maps USA constants to Japan ones; unvoted
literals take the delta of agreeing neighbours, since modules move as blocks
(CRI ADXM code +0x2A000, its data -0x718640). Of 519 distinct USA literals in
`recomp-runtime`, 79 are hooks, 69 are not guest addresses, about 140 map
with medium or high confidence, and about 230 have no proposal. Most of those
are DOAXBV-only code or opt-in debug watches, not DOA3 paths.

## Results

| Boot | Change | Result |
|---|---|---|
| 1 | first lift | stalls in `Direct3D_CreateDevice` (3925 `CDevice` offsets) |
| 2 | miniport and frame layouts | clears CreateDevice; stops calling CRI vblank worker A (not translated) |
| 3 | lookup translation | first CRI threads run; stops in CRI ADXM server (USA addresses) |
| 4 | CRI ADXM addresses | first frame presented, fibers run; crashes in `SetStreamSource` adapter |
| 5 | D3D state, draw and game seams left lifted | runs 120 s without a stop: 525k kernel calls, fills the `Z:` cache with `voice.afs` and `bgm.afs` |
| 6 | same build, 60 s paced | 60 fps steady (3541 presents, no late frames); restarts the `Z:` cache fill, which a killed run never finished |
| 7 | same build, `RECOMP_UNPACED=1`, 240 s | finishes the cache (`loadfile.afs` 438,065,152 bytes, copy thread exits), then the main loop runs 1.15M frames with no stop, no file reads and no audio buffers |

After the cache, the main fiber's per-frame function (Japan 0x000A6450, USA
0x000A02A0) runs Swap and the sound update every frame, but the game state
stops advancing. Finding the wait needs the draw path or a scripted START.

## Remaining work for a Japan target

- Port the 17 D3D state and draw adapters to D3D8 4134: device offsets, the
  separate dirty mask, and the 0x114-byte vertex shader record (the DOAXBV
  4928 branches share that record).
- Re-derive the 5 game seams on Japan code: the boundary and collision passes
  and the save-journal auto-save.
- Find why the game state stops after the cache fill.
- Apply the address map to the remaining adapters, and re-check each USA
  feature (saves, audio, movies, collision, pacing) on Japan.