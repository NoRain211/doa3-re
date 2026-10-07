# DOA3 native PC port

Native PC port of Dead or Alive 3 (Xbox, USA). Same approach as
[doaxbv-re](https://github.com/NoRain211/doaxbv-re): the pinned
`tools/xboxrecomp` lifter turns a user-owned XBE into local C, which runs on
the hand-written runtime in `recomp-runtime/`.

## Status

First whole-program run: 7,282 functions lifted, and the run stops in CRT
startup on a lift gap in `memcpy`'s jump tables. See
[docs/bring-up.md](docs/bring-up.md).

`recomp-runtime/` is copied from doaxbv-re at `f6ad13e`. Its kernel, models
and D3D11 presenter are game-independent. Its `*_adapter.c` files,
`program_manual.c` and `program_adapters.c` still bind DOAXBV guest addresses
and must be re-found for DOA3 before use.

| Library | DOA3 | DOAXBV |
|---------|------|--------|
| XAPILIB, D3DX8, XGRAPHC, XBOXKRNL | 3911 | 4928 |
| D3D8 | 3925 | 4928 |
| DSOUND | 3936 | 4928 |

Both games carry CRI Sofdec (`PSGSFD*`), XPP and DOLBY sections and ship AFS
archives and SFD movies.

## Display settings

The presenter's settings carry over unchanged: `RECOMP_D3D_SCALE` (1-8,
render height multiplier), `RECOMP_D3D_MSAA` (sample count) and
`RECOMP_D3D_SMAA=1` (needs `third_party/smaa` at build time). They take
effect once the D3D adapters are re-bound for DOA3.

Widescreen defaults off: DOA3's box lists no widescreen support, so it is
expected to render 4:3 in a 640x480 window. `RECOMP_D3D_WIDESCREEN=1` reports
the dashboard's widescreen flag; if the game ignores it, true 16:9 needs a
projection change in the game's camera code once it is lifted.

## Setup

```powershell
git submodule update --init
python tools/extract_iso.py <your-doa3.iso>        # -> private/imported-disc/disc
python tools/xbe_info.py private/imported-disc/disc/default.xbe
cmake -S recomp-runtime -B build/recomp-runtime
cmake --build build/recomp-runtime --config Debug
ctest --test-dir build/recomp-runtime -C Debug --output-on-failure
```

`extract_iso.py` needs XboxDev
[extract-xiso](https://github.com/XboxDev/extract-xiso/releases) on PATH or at
`tools/artifacts/extract-xiso.exe`.
