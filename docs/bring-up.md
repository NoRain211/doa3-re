# DOA3 bring-up

Status of the first whole-program run of Dead or Alive 3 (Xbox, USA) on the
recomp runtime. Everything derived from the game stays under `private/`.

## Lift

Run from `tools/xboxrecomp` (branch `codex/doa3-recipe`) against a copy of
`default.xbe` in `private/lift/`, so the parser's analysis JSON does not land in
the verified disc import. The lift used `fe9e410`; the current pin `59b34ff` adds
only lifter tests, so it generates the same code.

```powershell
python -m tools.xbe_parser ../../private/lift/default.xbe --json ../../private/lift/default_analysis.json --quiet
python -u -m tools.disasm ../../private/lift/default.xbe -o ../../private/lift/disasm -v
python -u -m tools.recomp ../../private/lift/default.xbe --all --split 1000 --game-name "Dead or Alive 3" `
  --functions ../../private/lift/disasm/functions.json --labels ../../private/lift/disasm/labels.json `
  --func-id-dir ../../private/lift/no-func-id --abi-dir ../../private/lift/no-abi `
  --icall-sites ../../private/lift/no-icall-sites.json `
  --gen-dir ../../private/lift/generated --output-dir ../../private/lift/metadata
```

Disassembly takes every code section (no `--text-only`), because D3D, DSOUND and
XPP code lives outside `.text`. No recipe, recoveries, manual functions,
func_id or ABI input were used; the empty paths keep stale lifter output out.

| Result | Value |
|---|---|
| Functions | 7,282 (.text 6,356, DSOUND 351, D3D 284, XPP 224, PSGSFD* 35, D3DX 20, XGRPH 12) |
| Translated / failed | 7,282 / 0 |
| Generated C | 1,521,489 lines in 8 chunks |
| Unresolved call targets | 129 |
| Untranslated instructions | 197 sites, 36 mnemonics, emitted as `RECOMP_UNIMPL` |
| Lifter warnings | identified and ABI inputs not found (intentional) |

Most untranslated sites (`aam`, `arpl`, `insb`, ...) are data decoded as code.
`in`, `out`, `wbinvd`, `cli` and `sti` are real hardware access.

## Build

The manifest is computed the way doaxbv-re's `build_game.py` does it: one
`name<TAB>size<TAB>sha256` line per `recomp_NNNN.c`, then `recomp_dispatch.c` and
`recomp_funcs.h`, hashed with SHA-256.

```powershell
cmake -S recomp-runtime -B build/recomp-program -G "Visual Studio 17 2022" -A x64 `
  -DRECOMP_PROGRAM_DIR=private/lift/generated `
  -DRECOMP_PROGRAM_MANIFEST_SHA256=<manifest sha256> `
  -DRECOMP_PROGRAM_EBP_EXPECTED=0
cmake --build build/recomp-program --config Debug --target recomp_program_runner
```

`RECOMP_PROGRAM_EBP_EXPECTED` is 0: upstream's emitter initializes `ebp` itself,
so the uninitialized prologue local that DOAXBV's build patched (11,052 times)
no longer occurs. Configure generates 169 fail-loud stubs: absent bodies, direct
calls to garbage addresses from data decoded as `call`, and DOAXBV bodies named
by the gated adapters.

Upstream also writes its own `recomp_types.h` beside `recomp_funcs.h`. Configure
now copies `recomp_funcs.h` next to the derived chunks and leaves the snapshot
directory off the include path, so generated code includes the runtime's header.

## Runtime changes

- `recomp_lookup_manual()` returns NULL unless `RECOMP_DOAXBV_BINDINGS` is defined.
  Only `recomp_runtime_test` defines it, so adapter tests keep their DOAXBV
  addresses. The DOAXBV vibration default poke in `runner.cpp` is behind the same
  switch. Kernel imports are unaffected.
- Upstream emits direct calls as `RECOMP_ABI_CALL(va, sub_X)`, a plain call. Direct
  calls never reach `recomp_lookup_manual()`, so re-binding an adapter for DOA3
  needs `--manual-functions` plus a `sub_X` definition that forwards to it.
- `recomp_types.h` gained what the generated code uses: `RECOMP_ABI_CALL`,
  `RECOMP_ICALL_SAFE_AT`, `RECOMP_FP_PC` with `recomp_fp_round24`, `RECOMP_FCMP_CC`,
  `g_fp_cc`, `recomp_fxam`, `recomp_frndint`, `recomp_fist`, `RECOMP_DF_STEP` and
  `g_df`, `RECOMP_ATOMIC_CAS32`, `RECOMP_UNIMPL`, `xbox_ReadTimeStampCounter`,
  `ROL8`/`ROR8`/`RC_ROT`, `XMM_CMP_PRED`, and `XBOX_FS_BASE` (0, where the runner
  builds the TIB). MMX registers are now `RecompMmx` lanes in `RecompRuntime`,
  with the 15 MMX ops DOA3 emits; the old-lifter `MMX_CVTPS2PI` is gone.
  `RECOMP_UNIMPL_TRAP=1` stops at the first untranslated instruction reached.
- `recomp_debug_service` (`int 0x2d` DbgPrint) prints to stderr.
- Kernel data exports and the synthetic thread objects moved from
  `0x00740000` to `XBOX_KERNEL_DATA_BASE` (`0x00F80000`). The old block sat inside
  DOA3's `.data` BSS (`0x00219640`-`0x00C27F5C`). The runner now refuses an XBE
  image that reaches the block.
- `xbe/` (the runner's XBE parser) was copied unchanged from doaxbv-re.

## Current stopping point

```
runner --xbe <disc>/default.xbe --expect-stop indirect:0x0018e098 --milestone-log <log>
recomp stop: indirect:0x0018e098 result=match kernel_calls=8 kind=runtime-error
```

XAPI starts its main thread (`PsCreateSystemThreadEx start=0x00164c28`), 8 kernel
calls are serviced, and the first `memcpy` (`sub_0018DF40`, the CRT's assembler
memcpy) stops on an indexed jump to `0x0018E098`, which has no body. The run
exits in about half a second. Two runs, before and after the kernel data move,
stop identically; the only difference in their logs is a host-side timing line.

Cause: memcpy's three inline tables (`0x0018DFA0`, `0x0018E01C`, `0x0018E088`)
dispatch into `0x0018DFA8`-`0x0018E0D5`, and no lifted function owns any byte of
that range. The disassembler ended the function where it began decoding an
inline table as instructions. The translator's arm recovery
(`discover_jump_table_entries`) assumes a table follows the code it indexes, so it
reads these forward-pointing tables as empty and skips them. Entry 0 of the
trailing-byte table is the shared epilogue, so every memcpy stops here.

## D3D8 3925 and DSOUND 3936 entry points

Find these in DOA3, in this order, before the presenter can show a frame. The
"first signature" column is the earliest XDK build for which
[XbSymbolDatabase](https://github.com/Cxbx-Reloaded/XbSymbolDatabase) has a
signature; 3911 means it exists in DOA3's library generation. DOAXBV addresses
are XDK 4928.

| # | Entry point | First signature | DOAXBV adapter (address) |
|---|---|---|---|
| 1 | `Direct3D_CreateDevice` | 3911 | `d3d_creation_adapter.c` `recomp_d3d_create_device_adapter` (0x001E9100) |
| 2 | `CDevice::KickOff` (push-buffer kick-off) | 3911 | `d3d_creation_adapter.c` `recomp_d3d_kick_off_adapter` (0x001E9EB0) |
| 3 | push-buffer space request | 4034 as `D3D_MakeRequestedSpace`; find DOA3's equivalent | `d3d_creation_adapter.c` `recomp_d3d_make_requested_space_adapter` (0x001EA190) |
| 4 | `D3DDevice_Clear` | 3911 | `d3d_frame_adapter.c` `recomp_d3d_clear_adapter` (0x001E72D0) |
| 5 | `D3DDevice_Present` | 3911; `D3DDevice_Swap` starts at 4034 | `d3d_frame_adapter.c` `recomp_d3d_swap_adapter` (Swap, 0x001E8F30); Present takes 4 stack arguments where Swap takes 1 |
| 6 | `D3DDevice_DrawIndexedVertices` | 3911 | `d3d_draw_adapter.c` `recomp_d3d_draw_indexed_vertices_adapter` (0x001E78B0) |
| 7 | `D3DDevice_DrawVerticesUP` | 3911 | `d3d_draw_adapter.c` `recomp_d3d_draw_vertices_up_adapter` (0x001E7750) |
| 8 | `D3DDevice_SetVertexShader` | 3911 | `d3d_vertex_shader_adapter.c` `recomp_d3d_set_vertex_shader_adapter` (0x001E7170) |
| 9 | `D3DDevice_SetTexture`, `D3DTexture_LockRect` | 3911 | `d3d_texture_adapter.c` set_texture (0x001E43F0), lock_rect (0x001E8090) |
| 10 | `D3DDevice_SetRenderState_*` (Simple, EdgeAntiAlias, CullMode, NormalizeNormals, TextureFactor, FillMode, ZEnable, StencilEnable, StencilFail, MultiSampleAntiAlias) | 3911 | `d3d_render_state_adapter.c` (0x001E4D80-0x001E6510) |
| 11 | `D3DDevice_SetTile` | 3911 | `d3d_tile_adapter.c` `recomp_d3d_set_tile_adapter` (0x001E4930) |
| 12 | `D3DDevice_SetGammaRamp` | 3911 | `d3d_frame_adapter.c` `set_gamma_ramp` (0x001E3640) |
| 13 | `D3DDevice_Reset`, `D3DDevice_PersistDisplay` | 3911 | `d3d_creation_adapter.c` reset (0x001E3B00), persist_display (0x001E4AE0) |
| 14 | `DirectSoundCreate`, `DirectSoundDoWork` | 3911 | `dsound_service_adapter.c` create (0x001FA27C), do_work (0x001F90E0) |
| 15 | `CDirectSound` DownloadEffectsImage, SetMixBinHeadroom, CommitDeferredSettings, SetPosition, SetVelocity | 3911 | `dsound_service_adapter.c` (0x001F8F21, 0x001F8F48, 0x001F974F, 0x001F9DD4, 0x001F9E09) |
| 16 | `IDirectSoundBuffer` Play, Stop, StopEx, GetStatus, GetCurrentPosition, SetCurrentPosition, SetFrequency, Release, SetBufferData | 3911 | `dsound_service_adapter.c` buffer_* (0x001F8FD8-0x001F9E5E) |

Rows 1-7 are the minimum for a frame: a device, a working push buffer, a clear,
a present and the two draw calls DOAXBV uses. DOA3 may also draw through
`DrawVertices` or `DrawIndexedVerticesUP`, which have no DOAXBV adapter yet. Rows
8-13 make the frame correct. Rows 14-16 are audio and do not block frames.
