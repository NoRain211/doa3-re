# DOA3 D3D8 entry points and device layout

Dead or Alive 3 links D3D8 3925. These are its D3D entry points, globals and
device fields, found with [XbSymbolDatabase](https://github.com/Cxbx-Reloaded/XbSymbolDatabase)
signatures (3911, the newest at or below 3925) and call-graph evidence from the
lift's disassembly. A signature "match" means every `OV_MATCH` byte matched at
exactly one place in the D3D section (`0x001B0DE0`); xref entries were checked
by hand where noted. Call counts are `call` sites in `.text` (game code); no
`.text` code tail-jumps to any of them.

## Entry points

| Symbol | Address | Evidence | Confidence |
|---|---|---|---|
| `Direct3D_CreateDevice` | `0x001B4EC0` | 3911 signature (18 bytes). Only writer of the static device pointer `0x001C0800` into `g_pDevice`; defaults the push-buffer sizes, sets the refcount, calls `CDevice::Init` `0x001B94F0`, stores `*ppDevice`, `ret 0x18` (6 args). One game call | high |
| `CDevice::Init` | `0x001B94F0` | Called only by CreateDevice; allocates context and push buffer, programs the miniport, calls `InitializeFrameBuffers` | high |
| `CDevice::InitializeFrameBuffers` | `0x001B9130` | 3911 signature; thiscall, one stack arg (presentation parameters) | high |
| `CDevice::KickOff` | `0x001B88C0` | 3911 signature; xref `+0x1F` reads `g_pDevice` | high |
| `CDevice::MakeSpace` | `0x001B8B00` | 3911 signature. This is DOA3's replacement for `D3D_MakeRequestedSpace` (first signed at 4034): the push-buffer reservation helpers fall into it when space runs out. thiscall on the device, no stack args, returns the new put pointer in `eax` | high |
| reservation, fixed | `0x001B8DA0` | stdcall(device): calls MakeSpace when put >= threshold, returns put | high |
| reservation, sized | `0x001B8DC0` | stdcall(device, dwords): calls MakeSpace when put + dwords*4 >= threshold + 0x200 | high |
| `D3D_KickOffAndWaitForIdle` | `0x001B8CF0` | 3911 signature | high |
| `D3DDevice_Release` | `0x001B10D0` | Decrements refcount `+0x408`; on the last reference calls `0x001B9A00` (device teardown), clears `0xAE4` dwords of the device and zeroes `g_pDevice` | high |
| `D3DDevice_DrawIndexedVertices` | `0x001B3940` | 3911 signature; xrefs check: `g_pDevice` and `CDevice::SetStateVB` `0x001B7700`. Writes the caller's primitive into method `0x17FC` untranslated. 12 game calls | high |
| `D3DDevice_DrawVertices` | `0x001B38A0` | 3911 signature; calls `SetStateVB`. 9 game calls | high |
| `D3DDevice_DrawVerticesUP` | `0x001B3760` | 3911 signature; calls `CDevice::SetStateUP` `0x001B79F0`. 18 game calls, plus one from D3DX | high |
| `D3DDevice_DrawIndexedVerticesUP` | absent | No signature match, and `SetStateUP` has one caller (DrawVerticesUP). Not linked; 0 calls | high |
| `CDevice::SetStateVB` / `SetStateUP` | `0x001B7700` / `0x001B79F0` | 3911 signatures | high |
| `D3DDevice_Clear` | `0x001B3390` | 3911 signature; `ret 0x18`. 11 game calls, 2 library calls (Init, Reset) | high |
| `D3DDevice_Present` | `0x001B5850` | 3911 signature; `ret 0x10`. 4 game calls | high |
| `D3DDevice_SetGammaRamp` | `0x001B0E00` | 3911 signature; `ret 8`. 2 game calls | high |
| `D3DDevice_Reset` | `0x001B1290` | 3911 signature | high |
| `D3DDevice_PersistDisplay` | `0x001B2180` | 3911 signature | high |
| `D3DDevice_BlockUntilVerticalBlank` | `0x001B1130` | 3911 signature; vblank event at `+0x24F0` | high |
| `D3DDevice_SetRenderTarget` | `0x001B1350` | 3911 signature; xref gives render target at `+0x40C` | high |
| `D3DDevice_SetStreamSource` | `0x001B4230` | 3911 signature; xrefs give `g_Stream` | high |
| `D3DDevice_SetTransform` | `0x001B0EC0` | 3911 signature | high |
| `D3DDevice_SetVertexShader` | `0x001B45F0` | 3911 signature; xref gives `+0x470` | high |
| `D3DDevice_SetIndices` | `0x001B1EA0` | 3911 signature | high |

## Globals

| Global | DOA3 | DOAXBV 4928 | Evidence |
|---|---|---|---|
| `g_pDevice` | `0x001C3390` | `0x001F2978` | KickOff/draw xrefs; CreateDevice writes it |
| second device pointer | `0x001C3394` | | written beside `g_pDevice` by CreateDevice and Release |
| device object (static) | `0x001C0800`, size `0x2B90` | `0x001F3120`, `0x2C30` | CreateDevice; Release clears `0xAE4` dwords |
| push-buffer size default | `0x001C33A0` = `0x80000` | `0x001F5D54` | CreateDevice; Init sizes the buffer from it |
| kick-off size | `0x001C339C` = `0x8000`, then size/16 | `0x001F5D50` | CreateDevice; `0x001B86B0` overwrites it with size >> 4 |
| single-step flag | `0x001C0178` | `0x001F297C` | KickOff branches on it |
| pusher debug mirrors | `0x001C0160`-`0x001C0174` | | Init copies end, base, context, register base, channel+0x40, channel+0x44 |
| `g_Stream[16]` | `0x001C05C8`, 12 bytes each: stride +0, vertex buffer +8 | stride `0x001F2E20`, buffer `0x001F2E28` | SetStreamSource; Release clears `0x30` dwords |
| multisample type | `0x001C0598` | | InitializeFrameBuffers |

## Device fields (3925)

Each was read from the named DOA3 function. "4928" is what the DOAXBV
creation/draw adapters use for the same role.

| Offset | Role | 4928 | Evidence |
|---|---|---|---|
| `+0x0000` | push-buffer put | `+0x0000` | KickOff, MakeSpace, reservations |
| `+0x0004` | threshold (end of reservation - `0x204`) | `+0x0004` | MakeSpace, `0x001B86B0` |
| `+0x0008` | dirty-state flags | | SetTransform, SetStreamSource, SetVertexShader OR bits into it; Init ORs `0x7FF` |
| `+0x000C` | device flags (bit 2 alternate pusher, `0x800`, `0x1000`, `0x10` from behavior flags) | `+0x0008` | KickOff, MakeSpace, DrawIndexedVertices, CreateDevice |
| `+0x0010` / `+0x0014` | push-buffer base / end | `+0x0024` / `+0x0028` | Init, MakeSpace |
| `+0x0018` | last kicked put | `+0x002C` | KickOff |
| `+0x001C` | fence counter (init 5, +2 per fence) | `+0x0030` | `0x001B86B0`, `0x001B8970` |
| `+0x0020`, `+0x0024`.. | fence ring index (16 entries of 12 bytes) | | MakeSpace, `0x001B8970` |
| `+0x03E8` | wrap offset (put - base at wrap) | `+0x0044` | MakeSpace |
| `+0x03EC` | init 3 | | `0x001B86B0` |
| `+0x03F0` | context (`0x60` bytes, context[0] = 3) | `+0x0034` | Init, `0x001B86B0` |
| `+0x03F4` / `+0x03F8` | alternate pusher (flag bit 2) | | MakeSpace |
| `+0x0400` | alternate put | `+0x035C` | KickOff |
| `+0x0404` | GPU register base (copy of miniport word 0) | `+0x23C0` | Init; MakeSpace reads `+0x324C` from it |
| `+0x0408` | refcount | `+0x0500` (`0x001F3620`) | CreateDevice sets 1, Release |
| `+0x040C` / `+0x0410` | render target / depth surface pointers | `+0x21B4` / `+0x21B8` | SetRenderTarget xref; agent C (Clear, GetBackBuffer) |
| `+0x0470` / `+0x0474` | vertex shader / handle | `+0x0384` | SetVertexShader; Init sets `0x001C0688` |
| `+0x0478` | argument to SetStateVB | | DrawIndexedVertices |
| `+0x0880` + 64*n | transform n (`(n + 0x22) << 6`) | `+0x0810` | SetTransform |
| `+0x2144` / `+0x2148` | colour / multisample buffer memory | | InitializeFrameBuffers |
| `+0x214C` | frame-buffer count (BackBufferCount + 1) | `+0x21BC` | InitializeFrameBuffers |
| `+0x2150` | embedded frame-buffer surfaces, `0x18` bytes each | pointers at `+0x21C0`.. | InitializeFrameBuffers via `0x001B4CC0` |
| `+0x2168` | embedded multisample surfaces | | InitializeFrameBuffers |
| `+0x2198` / `+0x219C` | depth memory / embedded depth surface | pointer at `+0x21CC` | InitializeFrameBuffers |
| `+0x21B4` / `+0x21B8` | back-buffer width / height | | InitializeFrameBuffers |
| `+0x21BC` + 24*n | tile descriptors | | `0x001B1F30` |
| `+0x2304` | channel object (DMA put `+0x40`, get `+0x44`) | `+0x23BC` | Init, KickOff, MakeSpace |
| `+0x2308` | embedded miniport (word 0 = GPU register base) | | Init, KickOff, AvSendTVEncoderOption callers |
| `+0x24F0` | vertical-blank event | | BlockUntilVerticalBlank |
| `+0x2518` | copy of frame counter at kick-off | `+0x257C` | KickOff |
| `+0x2B60` | frame counter | `+0x2C10` | KickOff, SetGammaRamp, Init |
| `+0x2B64` | presentation interval | `0x001F2D84` (global) | InitializeFrameBuffers |
| `+0x2B84` / `+0x2B88` / `+0x2B8C` | context, context+0x40, context+0x20 | `+0x2C20` / `+0x2C24` / `+0x2C28` | Init |

3925 presentation parameters end at `+0x30` (`FullScreen_PresentationInterval`):
InitializeFrameBuffers and Init read `+0x00`-`+0x10`, `+0x20`, `+0x24`, `+0x28`,
`+0x2C`, `+0x30` and nothing beyond. There are no `BufferSurfaces` or
`DepthStencilSurface` fields; D3D allocates the frame buffers itself.

## Adapter status

`tools/doa3/manual_functions.json` lists the rebound addresses and goes to the
lifter as `--manual-functions`. The lifter then generates no body for them and
emits each call to them as indirect dispatch (`RECOMP_ICALL_SAFE`), which the
runner resolves through `recomp_lookup_manual()` first. The dispatch table
still names `sub_X`, so `recomp-runtime/program_forwards.c` defines each one;
it runs only when no adapter is bound and stops with `manual-unbound:<address>`.
The list holds Clear, Present and SetGammaRamp (`d3d_frame_adapter.c`) and
the GPU layer below `CDevice::Init` (`d3d_miniport_adapter.c`).

- **Creation.** Direct3D_CreateDevice, `CDevice::Init`, InitializeFrameBuffers
  and MakeSpace stay generated. The miniport calls (`0x001BB770`,
  `0x001BB1F3`, `0x001BB256`, `0x001BB595`, `0x001BB66A`, `0x001BA7D8`,
  `0x001BB8A8`, `0x001BBA96`), the pusher flush `0x001B8890` and KickOff
  `0x001B88C0` are replaced. The KickOff model sets DMA put and get to the
  kicked put and marks every fence and flip done, which is what lets the
  generated MakeSpace and fence waits return. The miniport keeps the real GPU
  register base, so any remaining register access stops at `memory:0xFD......`.
  Not yet replaced: `0x001BA960` (Reset, PersistDisplay), `0x001BBB58`
  (teardown), `0x001BC260`/`0x001BCC00`; none ran to the main loop.
- **Draw.** The adapter reads about 40 DOAXBV render-state and texture-stage
  globals (`0x001F2988`-`0x001F3D78`) and DOAXBV game addresses (camera and
  view tables), and submits through the frame adapter's presenter. Only the
  stream, vertex-shader and transform locations above are mapped so far.
