# Project Agent Rules

## Goal

Produce readable, hand-written source for a native PC port of Dead or Alive 3
(Xbox, USA). The whole-program recomp is a temporary scaffold: generated game
code runs on a hand-written runtime with native kernel, input, audio, and D3D8
replacements. Sister project:
[doaxbv-re](https://github.com/NoRain211/doaxbv-re) (DOAXBV, same engine
family, XDK 4928).

The first whole-program run stops in CRT startup. Current work closes lift
gaps until the game reaches its first frame, then binds the D3D8 and DSOUND
adapters to DOA3 addresses. `docs/public-status.md` states what the public
tree and local builds currently prove. `docs/building.md` defines the public
and local build routes.

## Architecture

The tracked runtime under `recomp-runtime/` owns guest memory, registers,
dispatch, kernel adapters, device models, D3D8 replacements, and host
presentation. The pinned `tools/xboxrecomp` submodule lifts a user-owned XBE
into an ignored local directory.

The runtime was copied from doaxbv-re. Its models are game-independent; its
adapters, `program_manual.c` and `program_adapters.c` still hold DOAXBV
addresses. DOA3 links XDK 3911 (D3D8 3925, DSOUND 3936), so re-find each
library function in DOA3 before enabling its adapter.

D3D8 is statically linked into the game image. Replace it at the API level
through `recomp_lookup_manual()`. Keep device models callable as plain
functions, independent of interception. Do not build an NV2A emulator beneath
the game.

Host presentation is D3D11 on a render worker thread. A new backend, such as
D3D12 ray tracing or Vulkan, must leave D3D11 working as the fallback.

## Decomp loop

1. Select a game-owned function that already runs.
2. Read its local generated form beside bounded static analysis.
3. Write clear source with real names, types, and control flow.
4. Register it in `recomp_lookup_manual()` so it wins over generated dispatch.
5. Run the same bounded gate and retain the change only when behavior holds.

Work in data or call clusters. Generated neighbors still depend on fixed guest
addresses and layouts, so redesign a structure only after replacing every
function that owns it.

## Hard rules

1. Never hand-edit generated C. Fix `xboxrecomp` and regenerate locally.
2. Never commit an XBE, generated game C, retail instruction bytes, extracted
   assets or filenames, saves, BIOS data, private paths, or run evidence.
3. Keep private inputs and output under `private/`; only
   `private/README.md` is public.
4. Report observed behavior. Forced state is a hypothesis, not a result.
5. Keep models separable from interception and host delivery details.
6. Replace library code such as D3D8, XAPI, CRT, and middleware wholesale;
   decompile only game-owned logic. Until a library is replaced, an adapter may
   wrap or call its generated functions at a documented seam. Record each seam
   in a comment beside the adapter's manual lookup and in the feature's `docs/`
   page, and treat wholesale replacement as open work.
7. Base every public branch on `origin/main`. A local branch that does not
   contain the public root commit `ab7bbfc` carries private history: never
   push it or merge it into a public branch. Port the change onto `main`.

## Progress and verification

A runtime iteration is bounded when its observable completes in under a
minute. It is forward when the program reaches a later natural event. A test,
receipt, refactor, or crash-free run alone is not gameplay progress.

Public-only changes must configure without private inputs. Before pushing a
public change, run the `public-ci` checks.

```powershell
python -m unittest discover -s tools -p "test_*.py"
python tools/public_export.py verify --require-public-tree
cmake -S recomp-runtime -B build/recomp-runtime
cmake --build build/recomp-runtime --config Debug
ctest --test-dir build/recomp-runtime -C Debug --output-on-failure
```

Changes tested with local generated code must also report the generated
program's manifest and the observed frontier. Assert the frontier with
`--expect-stop` as `docs/public-status.md` shows.

The user's play test of a named build accepts gameplay, rendering, and audio
fixes. An agent smoke run shows only that the build starts and presents
frames; report it that way.

After two attempts stop before the same required event, stop varying runtime
runs. Compare both receipts, identify the earliest divergence, state one
falsifiable mechanism, and test the smallest safe change.

## Lifter

Builds use only the `tools/xboxrecomp` submodule, pinned to the
`codex/doa3-recipe` branch of `NoRain211/xboxrecomp`: `codex/doaxbv-recipe`
(`7adaf21`) with upstream `sp00nznet/xboxrecomp` main (`1409a7d`) merged in,
upstream winning every conflict. Other local lifter checkouts do not feed the
build. Use upstream's interfaces (`--manual-functions`,
`--coalesce-functions`), not doaxbv-re's `--manual-call-targets` and
`--recover-functions`.

DOA3 lifter fixes are commits on that branch; take later upstream changes by
merging upstream main into it. Moving the pin also updates the
`tools/xboxrecomp` commit in `public-export.json`; record the lifter revision
that produced the current lift in `docs/public-status.md`. Offer general
fixes upstream as focused pull requests. Do not vendor `xboxrecomp` source or
generated output into this repository.

## Tool routing

- Static addresses, xrefs, or function bounds: use one bounded Ghidra query and
  record the binary identity.
- Real kernel or hardware behavior: use xemu as a live oracle.
- XDK or NV2A semantics: consult public Cxbx, nxdk, XbSymbolDatabase, or public
  headers, then implement independently.

## Branches, pull requests, and releases

- When reporting work, say where each change lives: a merged PR, a pushed
  branch, or an uncommitted worktree.
- Keep extra worktrees under `private/` or the Codex worktree directory.
  Remove a branch or worktree only after its tip is reachable from
  `origin/main` or a pushed branch and its uncommitted changes are accounted
  for.
- Before merging, address valid review comments and wait for `public-ci` to
  pass. PRs land as GitHub merge commits.
- A new tracked file must be added to the `include` list in
  `public-export.json`.
- Release notes and `CHANGELOG.md` name each real fix with its issue or PR.
  Update `docs/public-status.md` with what the release proves.

## Agent skills

### Issue tracker

Issues, specs, and Wayfinder maps live in GitHub Issues for
`NoRain211/doa3-re`. See `docs/agents/issue-tracker.md`.

### Triage labels

Use `needs-triage`, `needs-info`, `ready-for-agent`, `ready-for-human`, and
`wontfix`. See `docs/agents/triage-labels.md`.

### Domain docs

This is a single-context repository: use root `CONTEXT.md` and, when present,
system-wide ADRs under `docs/adr/`. See `docs/agents/domain.md`.

### Shared decisions

Host-shell decisions that do not depend on the game live in doaxbv-re's
roadmap ([#1](https://github.com/NoRain211/doaxbv-re/issues/1)): the
redistribution boundary
([#2](https://github.com/NoRain211/doaxbv-re/issues/2)), the semantic D3D8
presenter seam ([#6](https://github.com/NoRain211/doaxbv-re/issues/6)), and
native audio and movie decoding
([#8](https://github.com/NoRain211/doaxbv-re/issues/8)). Follow them here and
reopen them there; do not fork them into this tracker. Port game-independent
runtime fixes from doaxbv-re as they land.

## Governance

Changes to this file must keep the custody rules at least as strict. Propose
any self-initiated governance change before editing it.
