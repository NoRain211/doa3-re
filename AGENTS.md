# Project Agent Rules

## Goal

Produce readable, hand-written source for a native PC port of Dead or Alive 3
(Xbox, USA). The whole-program recomp is a temporary scaffold: generated game
code runs on the hand-written runtime in `recomp-runtime/` with native kernel,
input, audio and D3D8 replacements. Sister project:
[doaxbv-re](https://github.com/NoRain211/doaxbv-re) (DOAXBV, same engine family, XDK 4928).

## Architecture

The runtime was copied from doaxbv-re. Its models are game-independent; its
adapters, `program_manual.c` and `program_adapters.c` still hold DOAXBV
addresses. DOA3 links XDK 3911 (D3D8 3925, DSOUND 3936), so re-find each
library function in DOA3 before enabling its adapter.

D3D8 is statically linked. Replace it at the API level through
`recomp_lookup_manual()`. Keep device models callable as plain functions,
independent of interception. Do not build an NV2A emulator beneath the game.
Host presentation is D3D11.

## Hard rules

1. Never hand-edit generated C. Fix `xboxrecomp` and regenerate locally.
2. Never commit an XBE, generated game C, retail instruction bytes, extracted
   assets or filenames, saves, BIOS data, private paths, or run evidence.
3. Keep private inputs and output under `private/`; only
   `private/README.md` is tracked.
4. Report observed behavior. Forced state is a hypothesis, not a result.
5. Replace library code (D3D8, XAPI, CRT, middleware) wholesale; decompile only
   game-owned logic. Document any temporary seam beside its manual lookup.

## Lifter

`tools/xboxrecomp` is pinned to `NoRain211/xboxrecomp`, currently at the same
commit as doaxbv-re (`7adaf21`, `codex/doaxbv-recipe`). DOA3-specific lifter
fixes go on a `codex/doa3-recipe` branch of that fork. Do not vendor lifter
source or generated output.

## Checks

```powershell
python -m unittest discover -s tools -p "test_*.py"
cmake -S recomp-runtime -B build/recomp-runtime
cmake --build build/recomp-runtime --config Debug
ctest --test-dir build/recomp-runtime -C Debug --output-on-failure
```
