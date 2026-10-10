# Changelog

## 0.2 Alpha (experimental) — 2026-10-09

The game now looks much closer to the Xbox: fighters and stages are lit by the
game's own lights instead of drawn at full brightness, and the save survives a
crash in the middle of saving. A review of the runtime against the original
executable also fixed a set of timing, file and kernel details, and the
renderer picked up the runtime fixes from the sister project doaxbv-re.

### Added

- **Fixed-function lighting.** About 95% of DOA3's 3D draws use Direct3D's
  built-in lighting, which 0.1 skipped, so fighters and stages showed flat,
  full-bright textures. The renderer now evaluates it per vertex as the Xbox
  does: scene ambient and emissive light, up to eight directional and point
  lights with range and distance falloff, and specular highlights (most
  fighter materials use a specular power of 30-40). The first texture stage
  combines texture and lit color the way the game sets it up, so translucent
  materials take their transparency from the material.
- **Crash-safe saves.** The auto-save after matches now runs inside the
  runtime's save journal, which also covers the title-data folder where DOA3
  keeps its save. If the game crashes or a disk write fails during a save, the
  previous save is restored right away or on the next start. Tested with a
  normal save, a forced short write and a forced exit right after the write.
- **Variable refresh rate (opt-in).** `RECOMP_D3D_VRR=1` lets the game's own
  60 Hz timer pace a VRR display. See `docs/runtime-options.md`.

### Fixed

- **Omega's heat haze** in the final Story stage blends the previous frame, as
  the Xbox does, so its afterimage shows instead of cancelling itself out.
- Even 60 Hz pacing on high-refresh displays.
- Closing the window during a frame exits normally instead of reporting an
  error.
- Depth-of-field and other effects that shrink the frame average it like the
  Xbox when rendering above native resolution.
- Unbinding a texture now resets the format the next texture is compared
  with, so a texture of the same format no longer keeps stale state.
- Kernel timers run their callback when they expire, not when they are set.
- Directory searches keep their wildcard after the first result, and file
  reads, writes and hashes check the whole buffer the game passes.
- The stage-edge collision pass keeps its per-stage state between frames, as
  the original does.
- Reserving memory inside an existing block no longer clears it.
- 3D audio: an invalid position no longer plays a sound at full volume,
  Doppler limits apply only to 3D sounds, and the play position keeps moving
  when audio output is muted.
- After the boot fast-forward, frame pacing waits the right time on systems
  without high-resolution timers.
- Builds no longer print SMAA conversion warnings, and a frame-buffer texture
  the renderer cannot map is logged instead of silently drawn untextured.

### Tester flow

- `BuildGame.cmd` extracts again when you drop a different ISO or an earlier
  extraction never finished, and refuses to replace a folder that holds saves.
- `Play.cmd` prepares its copy of the disc in a temporary folder, so closing
  the first launch early no longer leaves a broken copy, and it stays open
  with an error when the game exits abnormally.
- Packaging checks its bundled tools before it writes the ZIP.

### Upgrading from 0.1

Extract 0.2 into a new folder and run `BuildGame.cmd` again; the generated
program changed (the auto-save is now hand-written), so a 0.1 build is
refused. To keep your progress, launch 0.2 once, close it, then copy
`private\play-disc\.recomp-storage` from your 0.1 folder over the new one.

## 0.1 Alpha (experimental) — 2026-10-09

The first public release of the playable whole-program recomp. Private test
builds 0.0.1 and 0.0.2 came before it; the fixes below the viewport fix came
after 0.0.2.

### Added

- **Playable game.** Intro movie, title, menus, Story, Time Attack, Watch and
  Sparring fights with audio, stage transfers, continue and attract mode run
  on the lifted program, paced to 60 Hz.
- **Hand-written collision.** The per-frame boundary pass, the danger-zone
  pass and their exemption predicates replace the lifted code, so fighters
  stop at walls and stage transfers work.
- **Audio.** DOA3's DirectSound 3936 buffers play through XAudio2, with 3D
  distance, Doppler and the Xbox's HRTF at its own gain.
- **Display.** Resolution scale, MSAA, SMAA (from the `third_party/smaa`
  submodule) and opt-in anamorphic widescreen.
- **Tester flow.** `BuildGame.cmd` builds from your own ISO or disc folder,
  `Launcher.cmd` picks display settings and volume, and `Play.cmd` plays.
  The boot legal notice is fast-forwarded.

### Fixed

- Audio static: GetCurrentPosition reports the position already sent to the
  device.
- 3D voices no longer clip.
- Fixed-function draws honor the game's viewport, so the select-screen
  portrait stays inside its box.
- The final Story stage against Omega shows its arena and fighters instead of
  a black screen: its full-screen haze pass now blends with constant alpha and
  samples the rendered frame.
