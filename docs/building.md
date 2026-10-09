# Building

Every route needs your own copy of the USA Xbox release. Keep the disc image,
the extracted files, the generated program and every build under `private/`.

## Public test route

The runtime, model and adapter tests build without game files or submodules:

```powershell
cmake -S recomp-runtime -B build/recomp-runtime -G "Visual Studio 17 2022"
cmake --build build/recomp-runtime --config Debug
ctest --test-dir build/recomp-runtime -C Debug --output-on-failure
```

This produces test executables, not the game runner.

## Extract a user-owned ISO

```powershell
git submodule update --init
python tools/extract_iso.py <your-doa3.iso>        # -> private/imported-disc/disc
python tools/xbe_info.py private/imported-disc/disc/default.xbe
```

`extract_iso.py` needs XboxDev
[extract-xiso](https://github.com/XboxDev/extract-xiso/releases) on PATH or at
`tools/artifacts/extract-xiso.exe`.

## Lift a generated program

Run from `tools/xboxrecomp` (branch `codex/doa3-recipe`) against a copy of
`default.xbe` in `private/lift/`, so the parser's analysis JSON does not land in
the verified disc import. The current lift uses `3764134` (trailing jump-table
fix) and the tracked manual list `tools/doa3/manual_functions.json`.

```powershell
python -m tools.xbe_parser ../../private/lift/default.xbe --json ../../private/lift/default_analysis.json --quiet
python -u -m tools.disasm ../../private/lift/default.xbe -o ../../private/lift/disasm -v
python -u -m tools.recomp ../../private/lift/default.xbe --all --split 1000 --game-name "Dead or Alive 3" `
  --functions ../../private/lift/disasm/functions.json --labels ../../private/lift/disasm/labels.json `
  --func-id-dir ../../private/lift/no-func-id --abi-dir ../../private/lift/no-abi `
  --icall-sites ../../private/lift/no-icall-sites.json `
  --manual-functions ../../tools/doa3/manual_functions.json `
  --gen-dir ../../private/lift-int/generated --output-dir ../../private/lift-int/metadata
```

Disassembly takes every code section (no `--text-only`), because D3D, DSOUND and
XPP code lives outside `.text`. No recipe, recoveries, func_id or ABI input
were used; the empty paths keep stale lifter output out.

## Build a local runner

The manifest is computed the way doaxbv-re's `build_game.py` does it: one
`name<TAB>size<TAB>sha256` line per `recomp_NNNN.c`, then `recomp_dispatch.c` and
`recomp_funcs.h`, hashed with SHA-256.

```powershell
cmake -S recomp-runtime -B build/recomp-program -G "Visual Studio 17 2022" -A x64 `
  -DRECOMP_PROGRAM_DIR=private/lift-int/generated `
  -DRECOMP_PROGRAM_MANIFEST_SHA256=<manifest sha256> `
  -DRECOMP_PROGRAM_EBP_EXPECTED=0
cmake --build build/recomp-program --config Debug --target recomp_program_runner
```

`RECOMP_PROGRAM_EBP_EXPECTED` is 0: upstream's emitter initializes `ebp` itself,
so the uninitialized prologue local that DOAXBV's build patched (11,052 times)
no longer occurs. Configure generates 168 fail-loud stubs: absent bodies, direct
calls to garbage addresses from data decoded as `call`, and DOAXBV bodies named
by the gated adapters.

Upstream also writes its own `recomp_types.h` beside `recomp_funcs.h`. Configure
now copies `recomp_funcs.h` next to the derived chunks and leaves the snapshot
directory off the include path, so generated code includes the runtime's header.

## Display settings

The presenter's settings carry over from doaxbv-re: `RECOMP_D3D_SCALE` (1-8,
render height multiplier), `RECOMP_D3D_MSAA` (sample count) and
`RECOMP_D3D_SMAA=1` (needs `third_party/smaa` at build time). They take
effect once the D3D adapters are re-bound for DOA3.

Widescreen defaults off: DOA3's box lists no widescreen support, so it is
expected to render 4:3 in a 640x480 window. `RECOMP_D3D_WIDESCREEN=1` reports
the dashboard's widescreen flag; if the game ignores it, true 16:9 needs a
projection change in the game's camera code once it is lifted.
