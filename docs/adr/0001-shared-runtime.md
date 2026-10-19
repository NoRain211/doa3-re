# Share one runtime across the sister ports

Status: accepted

`recomp-runtime/` was copied from doaxbv-re at `f6ad13e` and has drifted 150
runtime commits behind it. Fixes cross only by hand: the sync in PR #2 was
closed, and PR #7 ports doaxbv-re's macOS and Linux host layer a second time.
A third port, DOA2U, copied the runtime again from DOA3 and has since grown its
own presenter and models. We will move the game-independent parts (kernel,
save journal, presenter, audio and input hosts) into one shared runtime that
all three games consume. Each game keeps its own adapters, manual lookup
tables and timing choices, such as DOA3's guest-clock vblank pacing.

The games link three D3D8 and DirectSound versions: 3925/3936 (DOA3), 4928
(DOAXBV) and 5849 (DOA2U). Version-specific function addresses, object
layouts and state caches stay in each game's adapters; the shared presenter
takes only semantic commands. DOA2U already runs DOA3's presenter through its
own 5849 adapters, which shows the split holds.

The shared runtime lives in its own game-free repository. All three ports pin it as
a submodule, the way they pin `tools/xboxrecomp`.

Each part starts from the port that is furthest ahead in it. The kernel, save
journal and macOS and Linux hosts come from doaxbv-re, which already carries
the save fsync fixes. The presenter and the split device, light and
DirectSound models come from DOA2U, which already carries most of DOA3's
rendering work. The remaining additions from the other two ports are then
folded in, such as DOA3's table fog and doaxbv-re's texture-stage combiner.
Pacing stays a per-game choice.

## Considered options

- Keep the copy and pull fixes by hand: the current state, which is already
  losing fixes such as doaxbv-re's save fsyncs.
- One large sync, then scheduled syncs: the copy still drifts between syncs,
  and every sync repeats the PR #2 conflict work.
- doaxbv-re as the upstream: DOA3 changes would have to land through
  doaxbv-re's review and release cycle.
- One repository for all three games: merges three trackers, release lines and
  contributor bases to share one directory.
- doaxbv-re's runtime as the base for every part: chosen first, then replaced
  once DOA2U joined, because DOA2U's presenter is the most complete of the
  three and porting it onto doaxbv-re's would redo that work.
