# Changelog

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
