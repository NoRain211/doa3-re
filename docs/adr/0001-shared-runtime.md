# Share one runtime with doaxbv-re

Status: accepted

`recomp-runtime/` was copied from doaxbv-re at `f6ad13e` and has drifted 150
runtime commits behind it. Fixes cross only by hand: the sync in PR #2 was
closed, and PR #7 ports doaxbv-re's macOS and Linux host layer a second time.
We will move the game-independent parts (kernel, save journal, presenter,
audio and input hosts) into one shared runtime that both games consume. Each
game keeps its own adapters, manual lookup tables and timing choices, such as
DOA3's guest-clock vblank pacing.

The shared runtime lives in its own game-free repository. Both ports pin it as
a submodule, the way they pin `tools/xboxrecomp`.

It starts from doaxbv-re's runtime, which is newer and already carries macOS,
Linux and the save fsync fixes. DOA3's additions are then ported onto it:
fixed-function lighting, table fog, guest viewports, line lists and
previous-frame reads. Pacing stays a per-game choice.

## Considered options

- Keep the copy and pull fixes by hand: the current state, which is already
  losing fixes such as doaxbv-re's save fsyncs.
- One large sync, then scheduled syncs: the copy still drifts between syncs,
  and every sync repeats the PR #2 conflict work.
- doaxbv-re as the upstream: DOA3 changes would have to land through
  doaxbv-re's review and release cycle.
- One repository for both games: merges two trackers, release lines and
  contributor bases to share one directory.
