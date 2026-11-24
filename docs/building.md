# Building on macOS and Linux

The runner also builds natively for macOS (Apple Silicon or Intel) and Linux
x86-64, including SteamOS. It is the same lifted program and runtime as the
Windows build. Only the host layer differs:

| | Windows | macOS | Linux |
|---|---|---|---|
| Rendering | Direct3D 11, or Vulkan through SDL_GPU | Metal through SDL_GPU | Vulkan through SDL_GPU |
| Audio | XAudio2 | SDL3 (CoreAudio) | SDL3 (PipeWire, PulseAudio or ALSA) |
| Input | SDL3 gamepads, keyboard | same | same |
| Fibers and threads | Win32 | `win32_compat.c` and `fiber_switch_*.S` | same |

The macOS and Linux host layer comes from the sister project
[doaxbv-re](https://github.com/NoRain211/doaxbv-re), whose macOS and Linux
ports are shared code.

## Get the source

Download `DOA3-<version>-macOS.tar.gz` or `DOA3-<version>-Linux.tar.gz` from the latest
[release](https://github.com/NoRain211/doa3-re/releases/latest) and extract it
(for example `tar xzf DOA3-<version>-Linux.tar.gz`), then run the steps below from
the extracted folder. It holds the source, the pinned lifter and the scripts,
with no game files or prebuilt game. Instead of the archive you can clone the
repository with `git clone --recurse-submodules`.

The launcher's **UPDATE** page checks GitHub for a newer release when it opens and
unpacks it over the extracted folder (`private/` stays); run `./build_game.sh`
again afterwards. It needs a release archive with a `VERSION` file, so a checkout
is never updated.

## Build from the launcher

`./launcher.sh` opens on its GAME page. Choose
your disc with DISC ISO (an image) or DISC FOLDER (an extracted disc), or drop
either on the window, set MOVIES, and choose BUILD. The page shows the build's
latest line and keeps the full output in `private/build.log`; leaving while it
runs asks twice, then stops it. When it finishes, choose START GAME on the same
page.

The Linux package ships a prebuilt launcher, so this page needs nothing
installed. The build it starts needs the tools below, which `./install_deps.sh`
installs; on SteamOS it runs `build_steamdeck.sh`, which sets up its own container. On macOS the launcher
compiles on first use and needs the Xcode command line tools.

## macOS

Install CMake, Ninja, SDL3 and Python 3.12 or newer:

```bash
brew install cmake ninja sdl3 python@3.13
```

Homebrew has no `extract-xiso`, so `build_game.sh` builds the pinned
[XboxDev/extract-xiso](https://github.com/XboxDev/extract-xiso) release into
`private/extract-xiso` on its first run. One on `PATH`, or one you name with
`--extractor`, is used instead.

The lifter needs Capstone 5.0.7. Homebrew's Python refuses a global
`pip install`, so put it in the virtual environment the scripts look for
(from a clone, as here, or from the extracted archive):

```bash
git clone --recurse-submodules https://github.com/NoRain211/doa3-re.git
cd doa3-re
python3.13 -m venv private/venv
private/venv/bin/pip install capstone==5.0.7
```

Then build from your ISO or extracted disc folder:

```bash
./build_game.sh /path/to/doa3.iso
```

Lifting takes several minutes. The generated program is checked against the
same manifest hash as the Windows build, so a different lifter or input stops
the build before it compiles.

`./launcher.sh` opens a window to pick resolution, anti-aliasing, volume and
widescreen, saves them to `private/launcher.json` and starts the game.
Building another disc version adds it to the **VERSION** row on the GAME page;
**START GAME** plays the version you pick, which the launcher remembers. Each
version keeps its own runner folder (`build/recomp-program` for USA 3.0,
`build/recomp-program-jp` for Japan 3.1), so building one never replaces
another. `./run_game.sh` starts the game with the saved choices and the
picked version; `./run_game.sh --version "Japan 3.1"` plays that version
instead. As on Windows, the game keeps its cache and saves in
`private/play-disc` (USA 3.0) and `private/play-disc-jp` (Japan 3.1), and
each session's log in `private/play-logs`.

## Linux and SteamOS

Linux renders with Vulkan and compiles shaders at runtime with shaderc, so the
host needs `libshaderc_shared.so.1` (SteamOS includes it) or, on Debian and
Ubuntu, `libshaderc.so.1` from the `libshaderc1` package.

Build in an x86-64 distribution with CMake, Ninja, a C++17 compiler, Git,
Python 3.12 or newer with the `venv` module, SDL3's build dependencies, and
`bsdtar` to unpack the RAR costume pack.
`./install_deps.sh` installs them with apt, dnf or pacman (`--print` shows
the commands instead). Then build:

```bash
./install_deps.sh
./build_game.sh /path/to/doa3.iso
```

`build_game.sh` checks for those tools first. Run in a terminal, it runs
`install_deps.sh` when one is missing; otherwise, as from the launcher, it
names the missing tools and stops. A missing `bsdtar` stops no build: in a
terminal it runs `install_deps.sh` for it, and if that fails, or away from a
terminal, the build skips the RAR costume pack and says to install it.
It makes `private/venv` with Capstone
5.0.7, builds `extract-xiso` as on macOS, and builds SDL3 when the
distribution has none.

SteamOS has a read-only system and no compiler. On a Steam Deck, extract the
release archive under your home folder, open Konsole in Desktop Mode and run:

```bash
./build_steamdeck.sh /path/to/doa3.iso
```

It installs distrobox into `~/.local`, creates an Ubuntu 24.04 container named
`doa3-build` that shares your home folder, runs `install_deps.sh` and then
`build_game.sh` there. The ISO must be under your home folder too. The finished
checkout runs on SteamOS itself. In Desktop Mode, `./launcher.sh` does the same
from its GAME page, and `./build_game.sh` on SteamOS runs this script.

`./launcher.sh` opens a gamepad-friendly settings screen, then starts the game;
`./run_game.sh` starts the game with the saved choices.

For Steam Game Mode, add `launcher.sh` (or `run_game.sh`) with **Add a
Non-Steam Game** in Desktop Mode. Under Steam the game and launcher open full
screen. If one controller drives two player ports, turn Steam Input off for
that shortcut.

## Differences from the Windows build

- SMAA needs the Direct3D 11 presenter. The launchers offer MSAA only.
- `RECOMP_D3D_VRR` and `RECOMP_D3D_PRESENT_LOG` are Direct3D 11 features.
- `RECOMP_AUDIO_CAPTURE` records SDL's final mix as a float WAV and does not
  stop after 60 seconds.
- `RECOMP_HEADLESS=1` hides the window, for scripted runs.
- Every runner build, Windows included, defines `RECOMP_PORTABLE_MATH`, so
  `RCPSS` and `RSQRTSS` return the exact reciprocal on every host instead
  of Intel's table approximation and netplay peers compute the same bits. The
  result is within Intel's documented error, but not bit-identical to an Xbox.

## Runtime tests

On every platform, configuration downloads Dear ImGui 1.91.9, which draws
the swimsuit switch's and the free camera's panels, and checks its SHA-256;
set `FETCHCONTENT_SOURCE_DIR_IMGUI` to an unpacked copy to configure
offline.
A program build also downloads miniupnpc 2.3.3, which asks the router to
forward a netplay host's port, and checks its SHA-256 the same way
(`FETCHCONTENT_SOURCE_DIR_MINIUPNPC`).
Netplay pairs runners of one release and generated program, so a program
build reads the release from `VERSION` or the Git commit; outside a
checkout, set `RECOMP_NETPLAY_VERSION`.

```bash
cmake -S recomp-runtime -B build/recomp-runtime -G Ninja
cmake --build build/recomp-runtime
ctest --test-dir build/recomp-runtime --output-on-failure
```

These build test executables, not the game runner, and need no game files.
