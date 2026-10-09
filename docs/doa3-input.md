# DOA3 input bring-up

Injected START and A reach the game's processed input state, but no advance
beyond the initial outer main loop was observed. This does not establish that
a title screen is running: the renderer produces black frames, and the first
game task has no confirmed execution. No additional input binding was needed.

These checks used runtime base `5a54a8c` and the existing CRI-enabled generated
snapshot from lifter `4d276d3`. No generated code, lifter, or runtime source was
changed. Logs, captures, and the isolated disc view remain untracked.

## Reproduction

Build the runner as described in [bring-up.md](bring-up.md), using an absolute
`RECOMP_PROGRAM_DIR` and that snapshot's manifest hash and EBP count. Use a
separate build directory. Set `$Runner`, `$Xbe`, and `$LogDirectory` to absolute
local paths, with `$Xbe` inside an isolated disc view: the runtime creates
writable emulated disk storage beside it. The initial baseline used the shared
disc location; subsequent runs used isolated storage copied from that baseline.

Run one instance at a time, visibly minimized, with a unique name:

```powershell
function Invoke-InputRun {
    param([string]$Name, [string[]]$InputArgs, [int]$Watchdog = 25000)
    $env:RECOMP_WATCHDOG_MS = "$Watchdog"
    $env:RECOMP_PERF_COUNTER = '1'
    $arguments = @('--xbe', "`"$Xbe`"",
        '--expect-stop', "watchdog:$Watchdog",
        '--milestone-log', "`"$(Join-Path $LogDirectory "$Name.tsv")`"")
    $process = Start-Process -FilePath $Runner `
        -ArgumentList ($arguments + $InputArgs) -PassThru `
        -WindowStyle Minimized `
        -RedirectStandardError (Join-Path $LogDirectory "$Name.err") `
        -RedirectStandardOutput (Join-Path $LogDirectory "$Name.out")
    if (-not $process.WaitForExit(55000)) {
        Stop-Process -Id $process.Id -Force
        throw 'Runner exceeded 55 seconds'
    }
    if ($process.ExitCode -ne 0) { throw "Runner exit $($process.ExitCode)" }
}

# Select the deterministic neutral source, with a pulse beyond the run.
# Omitting input options selects live host input instead.
Invoke-InputRun baseline @('--input-start-pulse-at', '1000000000')
Invoke-InputRun start120 @('--input-start-pulse-at', '120')

$env:RECOMP_WATCH = '005e5f90'
$env:RECOMP_WATCH_HITS = '20'
Invoke-InputRun watch-start120 @('--input-start-pulse-at', '120') 10000
Invoke-InputRun sweep @(
    '--input-start-pulse-at', '1', '--input-start-pulse-at', '30',
    '--input-start-pulse-at', '300', '--input-start-pulse-at', '900',
    '--input-a-pulse-at', '60', '--input-a-pulse-at', '600',
    '--input-a-pulse-at', '1200', '--input-buttons-at', '450:0010')

$env:RECOMP_WATCH = '005e5a08'
Invoke-InputRun task-a600 @('--input-a-pulse-at', '600')
$env:RECOMP_WATCH = '004b8428'
Invoke-InputRun baseline50 @('--input-start-pulse-at', '1000000000') 50000
Remove-Item Env:RECOMP_WATCH, Env:RECOMP_WATCH_HITS
```

For a capture, set `RECOMP_D3D_FRAME_DUMP` to an untracked BMP destination and
`RECOMP_D3D_FRAME_DUMP_AT=300` before the run. Input pulses last one successful
port-0 sample, not one millisecond. START is digital `0x0010`; A is analog
button index 0 with value 255. `--input-buttons-at <poll>:<hexmask>` accepts
only digital bits `0x0001` through `0x0080`; A is not a high digital bit.

## Existing input path

`program_manual.c` already calls `recomp_input_lookup_manual()` in its DOA3
group, outside the DOAXBV gate. The addresses below also appear in
`program_forwards.c` and `tools/doa3/manual_functions.json`. Generated callers
use manual dispatch, so they reach `input_adapter.c`.

| XAPILIB 3911 API | DOA3 address | Game path |
|---|---|---|
| XInitDevices | `0x001E5D4C`, thunk `0x001E6953` | initialization from `0x000A0680` |
| XGetDevices | `0x001E6958` | controller initialization `0x0009EA60` |
| XGetDeviceChanges | `0x001E697A` | polling `0x0009EAF0` |
| XInputOpen | `0x001E6E3A` | initialization and insertion |
| XInputGetCapabilities | `0x001E6EBB` | immediately after successful open |
| XInputGetState | `0x001E70AD` | `0x0009EAF0`, call site `0x0009EB69` |
| XInputClose | `0x001E6EAF` | removal branch; not exercised here |
| XInputSetState | `0x001E711E` | feedback path `0x0009EDC0` |

The game uses device type `0x001E59E0`. Port 0 opens successfully. The
capabilities buffer starts at `0x005E5CD0`; the state buffer follows its
25-byte layout at `0x005E5CE9`. State is a packet number followed by the
18-byte Xbox gamepad structure. The game's reads agree with the adapter's
offsets: digital buttons at state +4 and analog buttons at state +6.

Each main-loop pass calls `0x0009EE80`, which polls through `0x0009EAF0`,
converts input in `0x0009EB90`, then handles feedback. Watching the aggregate
newly pressed mask at `0x005E5F90` observed `0 -> 0x10 -> 0` for START and
`0 -> 0x100 -> 0` for A. The sweep produced all eight expected press/release
pairs. This confirms game-side consumption, beyond merely parsing runner
options. The watch report's extra camera fields and native fiber handle are
DOAXBV diagnostics and are not DOA3 state evidence.

## Observations

| Run | Watchdog | Kernel calls at stop | Observed state |
|---|---:|---:|---|
| Neutral baseline | 25 s | 32,955 | outer main loop / Present |
| START at 120 | 25 s | 32,892 | same |
| START at 120, processed-input watch | 10 s | 13,992 | START press/release; same loop |
| START/A/digital sweep above | 25 s | 32,871 | eight press/release pairs; same loop |
| A at 600, task-status watch | 25 s | 32,892 | task 0 initialized to `0x11`, no later status change |
| Neutral, main-task initialization watch | 50 s | 64,434 | no change at `0x004B8428`; same loop |

All milestones matched their watchdog expectation; this means an intentional
diagnostic stop, not successful game progression. Watchdog samples named
`D3DDevice_Present` (`0x001B5850`) via `0x00153EF0`, called from the outer loop
`0x000A06D0`. Presentation remained about 60 fps. Captures at present 300 in
the sweep and extended baseline were black; the sweep BMP was 720 by 480,
with every color byte zero. No visual screen identity can be assigned.

After normalizing host paths, all six runs had identical sequences of 387
logged kernel records, including 109 open/create calls and 13 reads. These
records are a subset of the total kernel-call counter. There were eight AFS
open attempts and 36 SFD open attempts, all during common startup activity;
no pulse produced a new open or read. No SFD read was observed.

The common reads included a 512-byte emulated-disk read at offset 2048, a
25,728-byte read at offset 0, and these archive accesses (handles identify
the observations without publishing asset filenames):

| Handle | Read offsets | Length |
|---|---|---:|
| 329 | 0 | 2,048 |
| 425 | 0, 2,048, 4,096, 6,144 | 2,048 each |
| 341 | 0, 2,048, 4,096, 6,144, 8,192 | 2,048 each |
| 341 | 351,051,776 | 782,336 |

The earliest confirmed input-dependent difference was the processed button
mask. It did not lead to a different file sequence, task status, or sampled
call path. The small total kernel-count differences between equal-duration
runs are not evidence of progression.

## Remaining blocker to investigate

The first scheduled game task is `0x00084340`, installed by `0x000A0680`
through `0x0009E422`. Its task record is `0x005E5A08`, with fiber handle at
+8 and callback at +12. The scheduler `0x0009E67B` invokes `0x0009E747`,
which calls XAPI's fiber switch at `0x00164FEF`.

This library routine is still generated. Its guest implementation loads a
different ESP, restores registers, and returns through the new stack. The
generated implementation instead ends with an ordinary C return to its
original caller; it does not transfer control to the new stack's continuation.
That is a concrete limitation of lifting this library routine, and a likely
explanation for the enabled task never showing initialization or progress.
The input experiment did not fix or independently prove that causal link.

The appropriate next experiment is to rebind the existing native fiber
service at the DOA3 XAPI boundary, then repeat the neutral/pulse comparison:
CreateFiber `0x00164F50`, DeleteFiber `0x00164FDC`, SwitchToFiber
`0x00164FEF`, ConvertThreadToFiber `0x0016502E`. The TLS-index global read by
these DOA3 routines is `0x00B228C8`; the existing fiber adapter uses DOAXBV's
`0x003B5258` and must not be enabled unchanged. These addresses come from
the local call flow and routine bodies, not transplanted DOAXBV addresses.

No forced game state or speculative input binding was used. Fiber rebinding
and any lifter changes remain separate work.

## Checks

`python -m unittest discover -s tools -p "test_*.py"` passed (three tests,
one skipped). CMake configure and Debug build passed, and
`ctest --test-dir build/recomp-runtime -C Debug --output-on-failure` passed
14/14. The separately built whole-program runner produced the observations
above.
