# DOA3 fiber bring-up

The native fiber service reaches the first task wrapper, then stops on missing
generated callback `0x00084340`. The callback itself has not run.

The service now has separate DOA3 and DOAXBV binding parameters.
The DOA3 manual lookup, forwards and lift list contain marked FIBER blocks.
No generated C was edited; no game task has a hand-written replacement.

## Verified XAPILIB 3911 boundary

The local DOA3 disassembly establishes these entry points and their callers:

| API | Address | Stack arguments | Game callers |
|---|---|---:|---|
| CreateFiber | `0x00164F50` | 3 | `0x0009E422` |
| DeleteFiber | `0x00164FDC` | 1 | `0x0009E422`, `0x0009E482`, `0x0009E616`, `0x0009E747` |
| SwitchToFiber | `0x00164FEF` | 1 | `0x0009E552`, `0x0009E747` |
| ConvertThreadToFiber | `0x0016502E` | 1 | `0x0009E59C` |

CreateFiber calls the kernel stack allocator, uses a minimum stack size of
`0x3000`, rounds up to a page, and places a 16-byte object below the stack top.
DeleteFiber passes the object's two stack bounds to the kernel stack deletion
routine. Their return cleanup sizes are 12 and 4 bytes respectively.

| Object offset | Meaning |
|---|---|
| +0 | callback parameter / fiber data |
| +4 | stack top |
| +8 | allocation base; zero for the converted main thread |
| +12 | saved stack pointer |

SwitchToFiber reads the TLS index from `0x00B228C8`, indexes the TLS slots
through `fs:[4]`, and saves ESP to the outgoing object's +12 field. It loads
the incoming ESP, updates TLS block +4 to the new handle, restores the exception
list and callee-saved registers, then returns on that stack. ConvertThreadToFiber
uses the same TLS index, puts the main fiber at TLS block +8, and publishes it
at TLS block +4. Both clean up one stack argument.

The native service uses this object layout and TLS publication scheme while
Windows fibers preserve the suspended C continuations. It enters the callback
directly with a synthetic guest call frame, replacing XAPI's bootstrap stack.
The retained service represents the converted thread's stack top with the
adapter entry stack and treats a zero stack request as the minimum; DOA3's
observed task creation requests an explicit `0x4000` bytes. These are retained
service limitations, not claims of byte-for-byte XAPI object fidelity.

The scheduler supplies `0x0009E410` as the fiber entry and a task record as its
parameter. That entry calls the callback at record +12. For task zero the
record is `0x005E5A08` and the callback is `0x00084340`. The scheduler stores
the returned fiber handle at record +8.

The DOAXBV-only spin probe and queue repost experiment are compile-time gated
with the other DOAXBV bindings. They cannot read or modify DOA3 state.

## Verification

The existing fiber adapter test now runs both address sets with a nonzero TLS
index. It checks stack recycling, callback parameter delivery, two native
switch/yield cycles, TLS publication, ESP, EBX and exception-list restoration.
Only the selected game's TLS-index address is mapped in each test.

Required checks passed: Python discovery (three tests, one skipped), runtime
CMake configure and Debug build, and CTest (14/14).

## Observed runs

The runner was re-lifted with unchanged lifter `4d276d3`, the existing seeded
disassembly, this worktree's manual list and forwards, and no generated edits.
The result has 7,277 translated functions and zero translation failures, with
52 bodyless manual entries and two existing CRI wrappers. Build used an
absolute generated directory, its manifest hash, EBP count zero, VS 2022 x64
Debug, and a separate build directory.

Each run used a new isolated disc view and its own copy of emulated storage,
a visible minimized runner, a unique milestone log, a 25-second watchdog,
performance counters, a watch on `0x005E5A08`, and a 55-second kill limit.

| Run | Input configuration | Stop | Kernel calls |
|---|---|---|---:|
| Neutral confirmation | START pulse at poll 1,000,000,000 | `indirect:0x00084340` | 1,918 |
| START/A | START at 120, A at 600 | `indirect:0x00084340` | 1,918 |

Both confirmation milestones matched `--expect-stop indirect:0x00084340`.
The initial exploratory neutral run reached the same stop, but its placeholder
expectation did not match; the neutral confirmation corrected that harness
expectation without changing the runtime.

The task-status watch observed `0 -> 0x11`, with no later transition. The native
main-fiber handle was already published. CreateFiber returned a new handle for
entry `0x0009E410` with task record `0x005E5A08` as its parameter. Execution then
reached that wrapper's indirect callback call at `0x0009E416`, on the new guest
stack, with the correct record in EAX and target `0x00084340`. This establishes
native stack transfer and wrapper execution, but not callback execution.

After host-path normalization, both runs have identical sequences of 387 logged
kernel records, including 109 open/create calls and 13 reads. Those sequences
also match the previous input-only task-watch run. There are no new file opens
or reads. The earliest observed difference from that old run is native fiber
state at task initialization, followed by the callback dispatch failure instead
of continued presentation. Between the two new runs, only the configured input
schedule differs; no game-side divergence was observed before the stop. No
START/A delivery was demonstrated before this early failure.

The failure occurs before the watchdog fires, so there is no new watchdog
sample or sustained frame-rate observation. The failure site and generated
caller establish the scheduler/wrapper path. No new kernel import failure or
memory fault occurred, and no forced task state was used.

## Lifter blocker and hypothesis check

After the repeated stop, runtime changes stopped. A private check using the
pinned lifter's own decoder and `discover_static_indirect_targets()` reproduced
the missing callback without modifying the lifter or generated C:

- The game registers `0x00084340` with an immediate push at `0x000A06BE`.
- The preceding function ends at `0x00084336`; the next starts at `0x00084590`.
  The callback therefore occupies a discoverable gap in the function database.
- That gap decodes to 145 instructions, contains no return, and loops back to
  `0x00084430`. The next function is a regular call target, not a tail alias.
- Static callback discovery requires a return or fallthrough into a tail alias,
  so it rejects this non-returning task. Neither its body nor its dispatch entry
  exists in the regenerated snapshot.

This confirms the missing-body discovery hypothesis. The runtime has no model
for this game-owned callback, so it cannot be rebound like a library import.
The next work belongs in lifter discovery (or an explicitly reviewed seed),
followed by regeneration and another neutral/pulse comparison. Do not replace
this callback with a stub or edit generated C. No lifter or seed change was made
in the fiber work.
