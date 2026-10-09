# DOA3 D3D frame entry points

Clear, Present and SetGammaRamp in Dead or Alive 3 (Xbox, USA; D3D8 3925),
replaced by `recomp-runtime/d3d_frame_adapter.c`. Addresses come from
[XbSymbolDatabase](https://github.com/Cxbx-Reloaded/XbSymbolDatabase) 3911
signatures (`src/OOVPADatabase/D3D8/3911.inl`) matched against every code
section of the XBE, then checked against the disassembly and the game's call
sites.

| Function | DOA3 address | Stack args (`ret`) | Signature hits | Game callers | Confidence |
|---|---|---|---|---|---|
| `D3DDevice_Present` | `0x001B5850` | 4 (`ret 0x10`) | 1, D3D section | `sub_001539C0`, `sub_00153DA0`, `sub_00153EF0` | high |
| `D3DDevice_Clear` | `0x001B3390` | 6 (`ret 0x18`) | 1, D3D section | 10 game functions plus D3D-internal callers (Reset, `sub_001B94F0`) | high |
| `D3DDevice_SetGammaRamp` | `0x001B0E00` | 2 (`ret 8`) | 1, D3D section | `sub_00153900`, `sub_00153930` | high |

`D3DDevice_Swap` does not exist in DOA3. Its earliest signatures (4034, 4627)
match nothing, and the game flips only through Present. All three game Present
call sites push four zero arguments. Both SetGammaRamp sites pass flags 1 and a
768-byte ramp in `.data`.

Other facts found on the way:

- `g_pDevice` is `0x001C3390`. `Direct3D_CreateDevice` (`sub_001B4EC0`)
  stores the static device object `0x001C0800` there. DOAXBV's device is
  `0x001F3120` (`RECOMP_D3D_DEVICE_ADDRESS`, owned by the creation model).
- `CDevice_KickOff` (3911 signature) is `0x001B88C0`.
- `D3DDevice_GetBackBuffer` is `0x001B15B0`; `D3DDevice_GetRenderTarget` is
  `0x001B1850`, and the function after it at `0x001B1870` reads the depth
  surface the same way.

## Device fields

DOAXBV values are XDK 4928 and were what the adapter used before.

| Field | DOA3 3925 | DOAXBV 4928 | DOA3 evidence |
|---|---|---|---|
| current render target (pointer) | `+0x40C` | `+0x21B4` | read by Clear and GetRenderTarget, written by SetRenderTarget |
| current depth surface (pointer) | `+0x410` | `+0x21B8` | read by Clear and the depth getter |
| back buffer 0 | `device + 0x2150` (embedded surface) | pointer at `+0x21C0` | GetBackBuffer returns `device + 0x2150 + slot * 0x18`; Reset passes it to SetRenderTarget |
| auto depth surface | `device + 0x219C` (embedded surface) | pointer at `+0x21CC` | Reset passes it to SetRenderTarget when `+0x2198` (its contiguous memory) is nonzero |
| frame counter | `+0x2B60` | `+0x2C10` | Present increments it; SetGammaRamp and KickOff read it |
| flips completed | `+0x2518` | not used | KickOff copies `+0x2B60` here; only Present reads it, to wait while two frames are pending |

The adapter writes its frame count to `+0x2B60` after each present, as the
guest Present would. It leaves `+0x2518` alone because only the replaced Present
reads it.

## Adapter calling conventions

| Adapter | Arguments read | ESP on return | EAX |
|---|---|---|---|
| `recomp_d3d_clear_adapter` | Count, pRects, Flags, Color, Z, Stencil | entry + 28 | preserved |
| `recomp_d3d_present_adapter` | pSourceRect, pDestRect (both must be NULL) | entry + 20 | 0 (`D3D_OK`) |
| gamma (`set_gamma_ramp`) | Flags (ignored), pRamp (768 bytes) | entry + 12 | preserved |

DOA3's Clear reads its arguments in the same order and pops the same 24 bytes
as DOAXBV's, and SetGammaRamp copies `0xC0` dwords from its second argument,
matching the existing gamma adapter. Present returns 0 on the normal path
(`+0x2514` clear); the first-present display setup it otherwise does belongs to
the host presenter.

Present runs the same model path the DOAXBV Swap adapter did:
`recomp_d3d_frame_present()` validates the rects and calls
`recomp_d3d_frame_swap(state, 0)`, so a Present yields the same presenter
command as a plain Swap.

## Unverified

- Surface layout. `recomp_d3d_texture_adapter_describe()` matches the 3925
  create encoder (`0x001BA530`) for Common, Data (26 bits), Lock, the format
  byte, mip count, log2 size and the linear Size word. It does not decode the
  depth, cube or dimension bits, so cube and volume textures would decode as
  2D. Back-buffer Size and Format were not checked against
  `InitializeFrameBuffers`.
- `KeTickCount` advances 16 ms per present. DOA3's only reader
  (`0x00164400`) stores it to `0x0085B9C8` and a CRI stopwatch, and nothing
  reads either for a decision, so this does not change behavior.
- The transform dump (`RECOMP_XFORM_DUMP_AT`) runs only when the variable is
  set and reads `device + 0x880 + 0x40 * state` (`SetTransform`
  `0x001B0EC0`): slot 0 view, 1 projection, 6 world.
- Nothing here has run against DOA3 yet; the whole-program run still stops in
  CRT startup before Direct3D is created.
