# Project Agent Rules

## Goal

Produce readable, hand-written source for a native PC port of Dead or Alive 3
(Xbox, USA). The whole-program recomp is a temporary scaffold: generated game
code runs on the hand-written runtime in `recomp-runtime/` with native kernel,
input, audio and D3D8 replacements. Sister project:
[doaxbv-re](https://github.com/NoRain211/doaxbv-re) (DOAXBV, same engine
family, XDK 4928).

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

`tools/xboxrecomp` is pinned to the `codex/doa3-recipe` branch of
`NoRain211/xboxrecomp`: `codex/doaxbv-recipe` (`7adaf21`) with upstream
`sp00nznet/xboxrecomp` main (`1409a7d`) merged in, upstream winning every
conflict. Use upstream's interfaces (`--manual-functions`,
`--coalesce-functions`), not doaxbv-re's `--manual-call-targets` and
`--recover-functions`. DOA3 lifter fixes are commits on that branch; take
later upstream changes by merging upstream main into it. Do not vendor
lifter source or generated output.

## Checks

Run these before pushing; `public-ci` runs the same set.

```powershell
python -m unittest discover -s tools -p "test_*.py"
python tools/public_export.py verify --require-public-tree
cmake -S recomp-runtime -B build/recomp-runtime
cmake --build build/recomp-runtime --config Debug
ctest --test-dir build/recomp-runtime -C Debug --output-on-failure
```

Moving the lifter pin also updates the `tools/xboxrecomp` commit in
`public-export.json`. A new tracked file must be added to its `include` list.

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
reopen them there; do not fork them into this tracker.
