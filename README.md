<div align="center">

# DOA3 Native PC Port

**A native Windows port of _Dead or Alive 3_, built by static recompilation, with the goal of rewriting it into readable source.**

[![CI](https://github.com/NoRain211/doa3-re/actions/workflows/public-ci.yml/badge.svg)](https://github.com/NoRain211/doa3-re/actions/workflows/public-ci.yml)
[![License: GPL-3.0-or-later](https://img.shields.io/badge/license-GPL--3.0--or--later-blue)](LICENSE)
![Platform: Windows x64](https://img.shields.io/badge/platform-Windows%2010%2F11%20x64-0078D6)

[Status](docs/bring-up.md) ·
[Report a bug](https://github.com/NoRain211/doa3-re/issues)

</div>

> [!IMPORTANT]
> This project ships **no game files**. You need your own legally obtained copy
> of the USA Xbox release. It is not affiliated with or endorsed by the game's
> rights holders.

## About

The original Xbox executable is lifted to C with a static recompiler, then
linked against a hand-written runtime that replaces the Xbox kernel imports
and, one entry point at a time, the Direct3D 8 and DirectSound APIs with native
Windows equivalents. The rest of the game, including its statically linked
libraries, runs as generated code. The plan is to replace game logic with
readable, hand-written source one data-structure cluster at a time once the
game runs.

The runtime comes from the sister project
[doaxbv-re](https://github.com/NoRain211/doaxbv-re), which is fully playable on
the same approach.

## Status

> [!NOTE]
> **Not yet playable.** The first whole-program run lifts 7,282 functions and
> stops in CRT startup, before the first frame. See
> [bring-up](docs/bring-up.md).

| Area | State |
| --- | --- |
| Whole-program lift | 7,282 functions, none failed |
| Kernel imports | Shared with doaxbv-re |
| CRT startup | Stops in `_initterm` on a static initializer without a body |
| D3D8 SetGammaRamp, Clear, Present | Bound to DOA3 addresses |
| Device creation, push buffer, draws | Found or pending; not bound |
| DirectSound 3936 | Not bound |
| Hand-written game logic | None yet |

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

## Build the tests from source

The runtime tests build without any game files or submodules:

```powershell
git clone https://github.com/NoRain211/doa3-re.git
cd doa3-re
cmake -S recomp-runtime -B build/recomp-runtime -G "Visual Studio 17 2022"
cmake --build build/recomp-runtime --config Release --parallel 2
ctest --test-dir build/recomp-runtime -C Release --output-on-failure
```

This produces test executables, not the game runner. Passing tests does not
establish game accuracy.

## Set up a local build

```powershell
git submodule update --init
python tools/extract_iso.py <your-doa3.iso>        # -> private/imported-disc/disc
python tools/xbe_info.py private/imported-disc/disc/default.xbe
```

`extract_iso.py` needs XboxDev
[extract-xiso](https://github.com/XboxDev/extract-xiso/releases) on PATH or at
`tools/artifacts/extract-xiso.exe`. The lift and runner build are in
[docs/bring-up.md](docs/bring-up.md).

## Display settings

The presenter's settings carry over unchanged: `RECOMP_D3D_SCALE` (1-8,
render height multiplier), `RECOMP_D3D_MSAA` (sample count) and
`RECOMP_D3D_SMAA=1` (needs `third_party/smaa` at build time). They take
effect once the D3D adapters are re-bound for DOA3.

Widescreen defaults off: DOA3's box lists no widescreen support, so it is
expected to render 4:3 in a 640x480 window. `RECOMP_D3D_WIDESCREEN=1` reports
the dashboard's widescreen flag; if the game ignores it, true 16:9 needs a
projection change in the game's camera code once it is lifted.

## Repository layout

| Path | Contents |
| --- | --- |
| [`recomp-runtime/`](recomp-runtime) | Runtime, kernel and input adapters, audio, D3D8 replacements, presentation, tests |
| [`xbe/`](xbe) | XBE parsing and hashing |
| [`tools/`](tools) | ISO extraction, XBE info, lifter submodule, DOA3 manual-function list, export checks |
| [`docs/`](docs) | Bring-up status, lift and build steps, D3D8 research |
| `private/` | Ignored local game inputs, generated output and run evidence |

## Contributing

Read the [contribution guide](CONTRIBUTING.md) first. Translation defects are
fixed in the lifter and regenerated; generated game C is never patched by hand.

When [reporting a bug](https://github.com/NoRain211/doa3-re/issues), include
your build version, steps to reproduce, what happened, your hardware and
controller, and the stop or crash message
([how to read logs](recomp-runtime/README.md#reading-stop-and-crash-logs)).
Remove private paths from logs, and never upload game binaries, generated game
C, assets, extracted filenames, saves, BIOS data or private run evidence.

## AI assistance

This project uses large language models and AI coding agents extensively for
code, reverse-engineering analysis, debugging, tests and documentation. Their
output can contain mistakes; changes are reviewed and tested, and unverified
behavior is tracked as such.

## License

Original code and documentation are licensed under
[GPL-3.0-or-later](LICENSE); see also [NOTICE](NOTICE). Third-party components
keep their own licenses. The license grants no rights to the game or its assets.

## Support

If you like this work, consider [buying me a coffee on Ko-fi](https://ko-fi.com/norainsrecomps).
