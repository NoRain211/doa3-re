# Runtime options

Environment variables and command-line flags read by
`recomp_program_runner`. An unset variable means off unless the table says
otherwise. Diagnostics print to stderr.

## Player settings

`Launcher.cmd` sets these, then starts `Play.cmd`.

| Variable | Effect |
|---|---|
| `RECOMP_D3D_SCALE` | Render height multiplier, clamped to 1-8. 3 renders 1440 lines natively. The launcher sets height / 480. |
| `RECOMP_D3D_MSAA` | MSAA sample count, clamped to 1-32. The GPU must support the count. |
| `RECOMP_D3D_SMAA` | Any value except `0` enables SMAA. |
| `RECOMP_D3D_WIDESCREEN` | Any value except `0` reports the dashboard's 16:9 setting to the game and sizes the window at 16:9. The 3D view widens; the HUD stretches. |
| `RECOMP_AUDIO_GAIN` | Master gain from 0 to 1, default 1. 0 or an invalid value mutes output. |

The launcher does not set this one:

| Variable | Effect |
|---|---|
| `RECOMP_D3D_VRR=1` | For variable-refresh displays: the game's own 60 Hz timer paces frames instead of the display's refresh. |

## Test harness

| Variable | Effect |
|---|---|
| `RECOMP_UNPACED=1` | Skips host waits by advancing the shared guest clock, so the game runs as fast as the CPU allows. The window title ends in "(unpaced)". Audio drops buffers. `tools/play.ps1` clears it. |
| `RECOMP_WATCHDOG_MS` | After this many milliseconds, samples registers, dispatch frames and the guest stack from a host thread, then stops. `tools/play.ps1` clears it. |
| `RECOMP_PERF_COUNTER=1` | Shows FPS and frame time in the window title and reports late frames. |
| `RECOMP_UNIMPL_TRAP=1` | Stops at the first untranslated instruction reached. Without it the first 32 are reported and run as no-ops. |
| `RECOMP_SAVE_SHORT_WRITE_AT`, `RECOMP_SAVE_INTERRUPT_AFTER_WRITE` | Save fault injection: shorten, or interrupt after, save write number N (1-1024). |

## Capture

| Variable | Effect |
|---|---|
| `RECOMP_D3D_FRAME_DUMP` | Writes the back buffer to this BMP path. `_AT` picks the first present (default 1), `_COUNT` a burst of numbered frames (default 1), `_INTERVAL_MS` spaces them in host time. |
| `RECOMP_D3D_DRAW_CAPTURE` | Writes one present's draw inputs to this path. `_AT` names the present; with `_AT=0`, `_TRIGGER` names a file whose appearance starts the capture. `_FVF` captures the first draw with that vertex format. |
| `RECOMP_D3D_PROGRAM_TEXTURE_DUMP` | Path prefix for textures used by programmable draws, gated by the draw-capture trigger. |
| `RECOMP_AUDIO_CAPTURE` | Records the final mix to this new file as IEEE-float WAV. An existing file is never overwritten. |
| `RECOMP_AUDIO_TRACE` | Logs DirectSound calls, including 3D position updates. |
| `RECOMP_INPUT_TRACE` | Logs host controller input. |

## Investigation traces

These were added for single investigations and are kept for reuse:
`RECOMP_FAULT_TRACE` (host stack on a guest fault), `RECOMP_WATCH` (hex guest
address whose writers are reported; `RECOMP_WATCH_HITS` sets how many,
default 40; it disables the fast memory path), `RECOMP_XFORM_DUMP_AT`,
`RECOMP_D3D_SLOTTRACE`, `RECOMP_D3D_RTTRACE`, `RECOMP_D3D_VIEWSEL`,
`RECOMP_D3D_VIEWARG`, `RECOMP_D3D_WHOCALLS`, `RECOMP_D3D_YTRACE`,
`RECOMP_D3D_YTRACE_DRAW` and `RECOMP_FIBER_FPU`.

`RECOMP_SPINPROBE` and `RECOMP_REPOST` came from DOAXBV and read DOAXBV
addresses; `RECOMP_REPOST` also changes behavior. `RECOMP_USER_MUSIC` and
`RECOMP_MUSIC_SHUFFLE` belong to DOAXBV's custom soundtrack, which DOA3
does not have.

## Runner flags

| Flag | Effect |
|---|---|
| `--xbe <path>` | The `default.xbe` to run; its folder is the disc root. Required. |
| `--vsync` | Also syncs presentation to the monitor. |
| `--expect-stop <id>` | Marks the run a match when it stops with this stop ID. |
| `--milestone-log <path>` | Appends the stop ID, result and kernel-call count to this file. |
| `--stop-at <id>` | Stops cleanly at a named boundary. The boundaries live in DOAXBV-bound adapters, so DOA3 does not reach them yet. |
| `--input-start-pulse-at <poll>`, `--input-a-pulse-at <poll>` | Presses START or A at that controller poll. Repeatable. |
| `--input-buttons-at <poll>:<hexmask>` | Presses digital buttons: `0x01`-`0x08` D-pad, `0x10` START, `0x20` BACK, `0x40`/`0x80` thumb clicks. Higher bits are refused. |
| `--input-analog-at <poll>:<index>:<value>` | Presses an analog button: 0 A, 1 B, 2 X, 3 Y, 4 Black, 5 White, 6 left trigger, 7 right trigger; value 0-255. |
| `--input-host-after-poll <poll>` | Hands input to the real controller after that poll. |
| `--input-wait-after-poll <poll>` with `--input-resume-file <path>` | Pauses the script after that poll until the file exists. |
| `--input-paused-analog-file <path>` | While paused, reads one press from this file and deletes it. |

Any input flag replaces live controller input with the script.
