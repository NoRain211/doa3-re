# Changelog

## Unreleased

### Fixed

- **Tag partners lost their swimsuits** (#150). Both women on a Tag
  Battle team shared one swimsuit, so the partner who tagged in went back
  to her DOA3 costume (or her first favorite), and her suit was lost again
  at every tag. Each partner now keeps her own swimsuit, starts in her own
  favorite, and the L3 menu changes the partner on screen.

## 0.8.1 — 2026-11-24

Mod costumes: the build adds two costume packs, and the game loads them all.

### Added

- **Mod costumes** in the USA and Japan builds. The build downloads
  Gultigargar's and usagiZ's costume packs from their MediaFire links once
  and adds as many costumes as the game can open, shared across the
  characters: about 103 beside the Booster Disc set. Your own costume files
  are never replaced, and a costume you delete stays deleted.
  `build_game.py --no-costumes` skips the download.

### Fixed

- **Mod costumes went missing** (#131). The runtime listed only the first
  64 files of a folder, so with the Booster Disc costumes installed, mod
  costumes past the 64th file name never loaded, and Kasumi through Zack lost
  all their extras. A folder now lists up to 4096 files, the Xbox limit; the
  game itself still reads at most 128 costumes.
- **Mod costumes past about 115 went missing.** The game keeps every costume
  file open, and the runtime allowed 256 open files, so past about 230
  costume files (115 costumes) the characters last by name lost theirs. It
  now allows 1024.

## 0.8.0 — 2026-11-23

The Europe disc, several versions side by side, a new launcher, faster
builds, smoother netplay and the swimsuit menu from the keyboard.

### Added

- **Europe 3.2 disc** (#128; Europe, Australia). It links XDK 4134 like
  Japan 3.1, so it builds from the Japan bindings at Europe addresses, mapped
  from the Japan recipe by function-size alignment and checked against the
  addresses Europe's own code reads ([Europe 3.2](docs/doa3-eur-test.md)).
  Story fights, menus, movies and audio run on Linux and the Steam Deck; the
  game seams stay lifted as on Japan, the main menu has no Quit Game entry
  yet, and the extras are untested on it. Thanks to @blueworkslabs for the
  port.
- **Several versions side by side** (#141). Each disc version builds into its
  own folder (`build/recomp-program`, `-jp`, `-eur`) and keeps its own saves,
  so building one no longer replaces another. The launcher lists every
  installed version and plays the one picked on the GAME page's Version row;
  `play.ps1 -Version "Japan 3.1"` and `run_game.sh --version` do the same.
- **Window controls** (#142). The Windows launcher has minimize, fullscreen
  and close buttons; fullscreen zooms the launcher to the screen. The macOS
  launcher can be minimized and resized.
- **Direct IPv6 path in netplay** (#134). When both players have IPv6, rooms
  try IPv4 and IPv6 together and keep whichever answers, which helps players
  behind carrier NAT. IPv4 stays the main path, and the relay still takes
  over when neither connects.
- **Python included on Windows** (#130). The Windows ZIP carries Python and
  Capstone, so building, playing and updating need nothing installed.
- **Linux build tools** (#137). `./build_game.sh` installs missing packages
  with apt, dnf or pacman (`./install_deps.sh`) and builds `extract-xiso`
  itself, on any distribution, not only the Steam Deck.
- **Swimsuit menu from the keyboard** (#127). Tab opens and closes the DOAXBV
  swimsuit menu, as L3 does on a controller, and the keys bound to LB, LT,
  RB, RT, X, B and Y (Q, E, W, R, A, X and S by default) work it.
  `private/keyboard.json` can move it with `"l3"`.

### Changed

- **New launcher layout** (#143). Settings are a list in a panel with five
  pages: GAME (Start Game, Version, DOA3 Disc, Movies, Build), VIDEO, AUDIO,
  GAMEPLAY and UPDATE. LB/RB, Q/E or Page Up/Down change page, up and down
  move between rows, left and right change a value, and START plays from any
  page. All three launchers have an icon (#144).
- **Faster builds** (#139). The lifter decodes each instruction once, the
  compile runs as many jobs as memory allows, and the ISO is hashed once. A
  full build took about 7.5 minutes instead of 13 on a 16-core test machine.
- **Smoother netplay** (#132, #133). Rollback restores and re-runs frames
  faster, and audio in netplay, replays and SyncTest plays on the real clock,
  so slow frames no longer crackle or bunch up sound. Stick, trigger and
  button values the game treats as zero are sent as zero, which avoids
  needless rollbacks.
- 0.7.x players cannot join 0.8.0 rooms.

### Fixed

- **A memory copy that could stop the game** (#138). The CRT's memcpy and
  memmove copy their last one to three bytes through a jump table whose first
  entry the lifter dropped. Europe's video settings Regulator screen hit it
  and exited; USA has the same table.
- **Swimsuit keys did nothing on Windows** (#127). 1 / 2, F5 / F6, F and G
  were read through SDL, which sees no keys in the Direct3D 11 window. They
  now go through the same input code as the game's keys.

## 0.7.1 — 2026-11-17

Graphics fixes: menu edges, Bayman's necklace and the widescreen portrait.

### Fixed

- **A one-pixel line along the top and left of the menus** (#119). Screen-space
  drawing was half a pixel off from the Xbox, so menu panels missed the first
  row and column. 2D now lands on the pixels the Xbox fills.
- **Bayman's necklace drew green and pink** (#119, #122). Its material lights
  diffuse twice, which the renderer skipped, falling back to a debug colour.
  It now renders as lit metal.
- **Squeezed character-select portrait at widescreen** (#122). The portrait
  was narrowed twice, so fighters looked tall and thin at 16:9 and wider. It
  now matches 4:3.
- **A one-time hitch the first time two vertex formats were drawn** (#120).
  Their shaders are now compiled at boot with the others. This does not
  address the recurring stutter reported there.

## 0.7.0 — 2026-11-16

Build from the launcher on Linux and macOS.

### Added

- **Build page in the Linux and macOS launchers** (#117). `./launcher.sh` opens
  on BUILD GAME while the game is not built: choose your disc with DISC ISO or
  DISC FOLDER, or drop it on the window, set Movies and choose Build. The page
  shows the build's latest line, keeps the full output in `private/build.log`,
  and asks twice before it stops a running build. START GAME on an unbuilt
  install sends you there.
- **A launcher that needs nothing installed on Linux.** The Linux package
  ships a prebuilt launcher (`tools/artifacts/launcher-linux-x86_64`, glibc
  2.34 or newer, x86-64), so the page works on a Steam Deck before any
  compiler exists. On SteamOS, Build runs `build_steamdeck.sh`. A checkout
  without it uses the launcher the game build makes, as before.
- **Steam Deck build script.** `./build_steamdeck.sh /path/to/doa3.iso` sets up
  a container with the build tools and runs the normal build there.

### Changed

- `build_game.sh` creates its own Python environment, so the build no longer
  stops on systems that refuse a global `pip install`.
- The Linux launcher's first two tabs are named VIDEO and AUDIO so all eight
  tabs fit.

### Fixed

- Updating from the Linux launcher failed with "text file busy" while the
  launcher itself was running. Files are now replaced by renaming over them.
- **Steam Deck build stopped at SDL's configure step** (#118). 0.6.7's
  `build_steamdeck.sh` did not install `libxtst-dev`, which SDL3 needs on X11.
  The script and the launcher's Build button on SteamOS now install it.

## 0.6.7 — 2026-11-16

Build on a Steam Deck in one step.

### Added

- **Steam Deck build script** (#117). `./build_steamdeck.sh /path/to/doa3.iso`
  installs distrobox into `~/.local`, creates an Ubuntu 24.04 container that
  shares your home folder, installs the build tools and `extract-xiso`, and
  runs the normal build there. SteamOS has no compiler, so this replaces
  setting up the container by hand. The finished install runs on SteamOS
  itself. The game is unchanged from 0.6.6.

## 0.6.6 — 2026-11-16

A smaller install.

### Changed

- **One copy of the disc** (#115). After a build, the play folder becomes the
  disc and the extracted copy is deleted, so Explorer reads about 5.5 GB for
  the install instead of about 10 GB. The disc was always on disk once; the
  play folder only linked to it, and Explorer counted every link. Older
  `private/program-*` folders are removed too. Your ISO, your own disc folder
  and extractions that hold saves are never deleted. After the next build,
  `private/play-disc` is the only copy: delete it and you need your ISO to
  build again.

## 0.6.5 — 2026-11-15

Online names, netplay across screen shapes, 3D voices without the echo, and
lit fighters on the USA disc.

### Added

- **Online name** (#111). NETPLAY SETTINGS gets a NAME (1-15 letters, digits
  and spaces), asked for the first time you host or join. Rooms show it, and
  online fights draw both names over the life bars. Players without one still
  show as PLAYER.

### Changed

- **Netplay across screen shapes** (#109). Online sessions and replays run the
  game at 16:9 and each window letterboxes it, so 4:3, 16:9 and ultrawide
  players see each other's rooms and stay in sync. Offline play keeps your
  screen shape. 0.6.4 players cannot join 0.6.5 rooms.

### Fixed

- **Echoing 3D voices** (#110, #108). Yells and other 3D sounds no longer echo
  or sound too open. HRTF taps, the reverb send and the reverb's shape now
  follow the hardware and the game's effect code. 3D voices are more
  directional and a little quieter when in front.
- **Overbright fighters on the USA disc** (#107). The 0.6.2 lighting fix for
  Hayate's white clothes and other glowing fighter materials only matched the
  Japan disc. They now take the stage's light on USA 3.0 too.

## 0.6.4 — 2026-11-14

In-launcher updates, Quit Game on the main menu, and faster frames.

### Added

- **Launcher updates** (#105). The Windows, macOS and Linux launchers get an
  UPDATE page that checks for the newest release and installs it, checked
  against GitHub's SHA-256 digest. Your `private` folder is kept. Check At
  Start (on by default) looks for an update when the launcher opens. After an
  update, build again to apply it to the game. 0.6.3 has no updater, so this
  release is a manual download; later releases can install from here.
- **Quit Game** (#106). The main menu ends with QUIT GAME, after the unlocked
  modes, on Japan 3.1 and USA 3.0. A on it opens the quit prompt.

### Changed

- **Faster frames** (#95–#99). Draws, the Direct3D 11 frame capture and
  indirect guest calls no longer allocate or repeat lookups every frame, and
  unchanged draw constants are not uploaded again. Ported from the DOA2U port;
  the DOA3 speed-up is unmeasured.
- **Sharper downscaled window** (#100). When `RECOMP_D3D_SCALE` makes the
  picture larger than the window, Direct3D 11 and SDL_GPU downscale it from
  mips instead of one bilinear tap.

### Fixed

- **Directory order** (#92). Every drive lists files to the game in NTFS
  order, so a game on an exFAT or FAT drive builds the same file tables as one
  on NTFS. In DOA2U the difference desynced every online session.
- **Game stays running after quit** (#101). On AMD's Direct3D 11 driver the
  process now exits after the window closes.
- **Drifting stick** (#102). A handheld stick that drifts no longer keeps the
  hold-Back quit prompt swallowing that pad's input.
- **Window focus** (#103). The game window takes the focus from the launcher,
  so a controller works without clicking the window first.

## 0.6.3 — 2026-11-14

Online rollback Versus, room reverb and modern water.

### Added

- **Rollback netplay** (#50–#55, #58, #77–#82). Press A on Versus in the main
  menu to host a room, join one by address or find one with FIND MATCH.
  When both players are ready, the game boots again in the same window and
  plays the session with rollback. Every session records a replay. Brought
  over from the DOA2U port with its later room, matchmaker and profile fixes.
  See `docs/rollback-netcode.md`.
- **Room reverb** (#36). Fight sound effects get the room reverb the game sets
  per stage, and a longer hallway tail during the KO slowdown. It
  approximates the Xbox effect, fitted to one Japan 3.1 stage preset; the
  longer USA presets and the effect's 24 kHz rate are unverified. Music and
  movie sound stay dry, as on the Xbox. See `docs/doa3-audio.md`.
- **Modern water** (#61, #56). Forest's creek and Beach's sea get reflections,
  refraction, caustics and foam on every renderer, and the straight seam
  across the Forest creek is gone. `RECOMP_D3D_WATER=0` keeps the game's own
  water.
- **Keyboard rebinding** (#83, #75) from `private/keyboard.json`.
- **F11 or Alt+Enter** switches between a window and borderless fullscreen
  (#85, #94).
- **F3** shows a frame rate readout (#86).
- **Hold Back to quit** (#87, #39). Holding Back for two seconds asks whether
  to quit to the desktop; a pending save finishes first.

### Changed

- **LB is White and RB is Black** (#84), as on the Xbox pad. They were the
  other way round.
- **Windows timer** (#91). The game asks for a 1 ms timer and turns off power
  throttling, so frame waits no longer miss on power-managed handhelds.
- **Launchers** (#60) load their backdrop from a file and carry version
  details, which calms some antivirus heuristics.

### Fixed

- **Resized 480p window** (#76) keeps the picture's aspect inside bars on
  Direct3D 11.
- **Launcher settings** (#88) are saved when the launcher closes, not only on
  START GAME.
- **Held Down in the launcher** (#38) no longer steps the first setting of the
  page it enters.
- **ISO in a non-ASCII folder** (#89) imports on Windows.
- **A failed ISO import** (#90) keeps the previous import.

## 0.6.2 — 2026-11-04

Lit white clothes and metal.

### Fixed

- **Overbright fighters** (#42, #49). Materials lit on a later texture stage,
  such as Hayate's white clothes and metal, no longer draw at full brightness
  on every stage. Thanks to YudoMatso for the fix.

## 0.6.1 — 2026-11-02

Faster builds.

### Changed

- **Faster builds** (#40). The build lifts from each disc's stored function
  list instead of disassembling `default.xbe` again, which saves 3 to 5
  minutes per build. The generated program is unchanged for both Japan 3.1
  and USA 3.0.

## 0.6 — 2026-11-02

Native 16:10 and ultrawide screens, and fullscreen that covers any screen.

### Added

- **16:10 and ultrawide screens** (#45). The launcher's Screen setting adds
  16:10 and Ultrawide (the primary monitor's own shape, such as 21:9 or 32:9,
  up to 4:1; 21:9 on a narrower monitor), which the game renders natively:
  its projection and fight camera take the shape, the HUD and menus keep
  their 4:3 shape, and movies keep the film's shape. Wider than 16:9, the
  fight camera keeps its 16:9 framing and the view widens around it.

### Fixed

- **Fullscreen on other screen shapes** (#45). On Windows above 480p, the
  game window covers the whole screen instead of leaving the desktop showing
  below or beside it. A screen of another shape than the chosen one gets
  black bars.
- **Japan 3.1 movies in widescreen** keep the film's shape instead of
  stretching.

## 0.5 — 2026-11-01

Windows builds without Visual Studio, and one beach launcher on every
platform.

### Changed

- **Windows builds without Visual Studio.** The build downloads the pinned
  [llvm-mingw](https://github.com/mstorsjo/llvm-mingw) 20261006 (Clang 23,
  the same compiler as the DOAXBV and DOA2U ports) and CMake 4.4.5 (245 MB,
  each checked against its SHA-256) into `private/` on its first run instead
  of installing the Visual Studio 2022 Build Tools, so Python is the only
  thing to install. The first 0.5 upload used w64devkit's GCC instead.
  The runner moves to `build/recomp-program/recomp_program_runner.exe`;
  rebuild once after updating. Visual Studio 2022 remains an optional way to
  build the runtime and its tests.
- **Beach launcher.** The Windows, Linux and macOS launchers share one look:
  a sunset beach between letterbox bars, with page tabs and the chosen page's
  settings side by side in the bottom bar. Left and right move between pages
  and settings. Down opens a page; inside it, up and down change a value. The
  Linux and macOS launchers move to pages too.

### Fixed

- **Save checks on Windows.** Save recovery no longer checks the bare drive
  name (`C:`, the drive's current folder) and reads file types from Windows
  attributes, so files dated before 1970 no longer stop recovery when the
  runtime is built with MinGW.
- **First build stopping with "Access is denied".** When antivirus is still
  scanning the new disc links or the extracted disc, the build waits up to 30
  seconds for the folder to unlock instead of failing on `play-disc.tmp`.

## 0.4 — 2026-10-27

A free camera for screenshots, chrome costumes that reflect the stage,
falling snow on SNOW, and exe launchers on Windows.

### Added

- **Free camera.** With **Free camera** ticked in the launcher (or
  `RECOMP_FREE_CAMERA=1`), F1, `` ` `` or a click of the right stick takes the
  game's camera and flies it through fights and cutscenes with the DOAXBV
  port's controls: the sticks or WASD and the arrow keys, a right-mouse
  drag to look, freeze (P, or Start while flying) and step the game, switch camera, reset,
  turn to a fighter and change her swimsuit, and a clean view that hides the
  HUD and the panel. F12, or the d-pad's right while flying, saves a
  screenshot of the game's frame to `DOA3` in the Pictures folder (the
  Windows known folder). No control uses Back. The game's culling follows
  the free camera, so nothing at the edges of the view drops out: the camera build (USA 0x0002A540, Japan
  0x00027720) is wrapped, not replaced, so both generated programs' hashes
  change. Works on Japan 3.1 and USA 3.0. Keys and the mouse are
  read through the input host on every platform; the SDL_GPU overlay panel
  resizes with the window. See `docs/free-camera.md`.

### Changed

- **The launcher and the game are exes** (Windows). `Launcher.exe` replaces
  `Launcher.cmd` and does everything it did: build, settings, drag and
  drop, controller. `DOA3.exe` starts the game with the saved settings.
  Neither needs PowerShell; both come from `tools/launcher.cs`, built with
  the C# compiler every Windows ships.

### Fixed

- **Chrome costumes reflect the stage.** Costumes whose texture the Xbox
  generates from the reflection vector, such as Zack's third, showed that
  texture stretched along the body; the reflection is now generated as the
  Xbox does on every backend.

- **Snow falls on SNOW.** Point lists, the game's particles, were dropped,
  so SNOW had no snow and AQUA PALACE lost its particles. Each point is now
  drawn as a camera-facing quad sized as Direct3D sizes point sprites, with
  depth and fog like the rest of the scene.

## 0.3.1 Alpha (experimental) — 2026-10-26

A fix for Windows players whose game stopped at the second start.

### Fixed

- **Second start with a save stopped at boot** (Windows). With a save in
  `TDATA`, the boot runs the legal notice and the save load without waiting,
  and a graphics card still busy with the previous frame made the game stop
  with `Present failed hr=0x887A000A`. That frame is now skipped instead,
  and the log notes the first skip once.

## 0.3 Alpha (experimental) — 2026-10-24

The Japan 3.1 disc becomes the main build, one launcher now builds and plays
the game and shows which version is installed, every DOA3 woman can wear her
DOAXBV swimsuits from the player's own DOAXBV disc, and the movies are
optional. The runner also builds on macOS and Linux.

### Changed

- **Japan 3.1 is the main build.** The build and the launcher take the Japan
  3.1 disc, with 3.1's moves, balance and costumes, as well as the USA 3.0
  disc, telling them apart by `default.xbe`. Each version keeps its own
  extraction (`private/imported-disc-jp`, `private/imported-disc`) and play
  folder with its saves (`private/play-disc-jp`, `private/play-disc`), so
  building one never touches the other. The launcher shows the installed
  version in its top left corner and on the BUILD GAME and START GAME pages,
  and the game log's first runner line names the disc (title, region,
  version and `default.xbe` hash).

- **One launcher builds and plays.** `Launcher.cmd` gains a **BUILD GAME**
  page: drop your DOA3 ISO or disc folder onto `Launcher.cmd` or the
  launcher window, or choose it there, set Movies and build, with the
  build's progress in the information line and its output in
  `private/build.log`. **START GAME** plays and sends you to the build page
  until a build exists. `BuildGame.cmd`, `Play.cmd` and `ExtractIso.cmd`
  are gone; `tools/build_game.py` and `tools/play.ps1` still run on their
  own. An Xbox controller now drives the launcher's menus: D-pad or left
  stick, A to select, B to go back.

- **Movies are optional.** The build page's **Movies** setting leaves out the
  18 movies, 3.4 GB of the 4.5 GB disc; the game then skips each one,
  including the intro. Building again with Movies on restores them.

- **The stick moves the fighter.** Analog Button is turned on at every boot,
  since DOA3 reads the stick only with it on, and a tap of a face button
  reads as a light press, so a tap of P stays a P instead of a hard →+P.

### Added

- **DOAXBV swimsuits.** With the player's own DOAXBV disc (an `.iso` or the
  extracted folder), every DOA3 woman can wear any of her DOAXBV swimsuits,
  switched live in a fight on USA 3.0 and Japan 3.1: click L3 on any
  controller to open the swimsuit menu, then LB / LT for P1 or
  RB / RT for P2, hold to scroll, with the suit's number and name shown under
  the life bars and a panel listing the controls. X / B marks a
  favorite and Y browses only favorites (saved per girl in
  `private/suit-favorites.txt`), and the launcher can start each fighter in
  her first favorite. Fighters keep their DOA3 heads, hair and animation; the
  neck turns with the head, joints bend as DOAXBV bends them, hands close
  with DOA3's fists, ankles stay smooth with the toes up, and skirts and
  tassels swing and clear the body as DOAXBV moves them. Suits are built
  at run time from the disc through recipes of numbers and disc locations
  (`recipes/doaxbv-suits.pack.gz`); no DOAXBV data ships. The disc is
  found in `private/` or a `doaxbv-re` checkout beside this one, or chosen
  or dropped in the launcher. See `docs/doaxbv-swimsuits.md`.

- **macOS and Linux builds.** The runner builds natively on macOS (Apple
  Silicon and Intel) and Linux x86-64, including SteamOS, with the host layer
  of doaxbv-re's ports: SDL_GPU rendering through Metal or Vulkan, SDL3 audio,
  POSIX fibers and file APIs. The SDL_GPU presenter carries DOA3's
  fixed-function lighting (point lights, specular, the stage-0 combine),
  table fog, the gamma ramp, the widescreen HUD at 4:3, guest viewports,
  line lists, constant-alpha blending, front-buffer reads and mipmapped
  frame copies above native resolution. `build_game.sh`,
  `launcher.sh` and `run_game.sh` build, configure and play; see
  `docs/building.md`. The release has `DOA3-0.3-Alpha-macOS.tar.gz` and
  `DOA3-0.3-Alpha-Linux.tar.gz` beside the Windows ZIP, with the same
  source and the scripts kept executable; extract one instead of cloning.

- **Skippable legal notice.** Any controller button skips the scrolling
  legal notice at boot.

### Fixed

- **Forest dust** draws as faint puffs instead of a solid brown blob.
- **DOATEC HK street:** wet-road reflections show only in puddles, and the
  road no longer flickers under the street lights.
- **Select screen:** at 16:9 the portrait stays inside its 4:3 frame, the line
  under names is gone, and portraits appear almost at once.
- **Performance:** fewer per-draw vertex copies and capture work off the
  game thread.
- **Swimsuit menu:** LB, RB, X, B and Y register again; buttons report a light
  press, which the menu used to ignore.
- **Windows build without Visual Studio:** the build no longer stops with a
  CMake "NMake Makefiles does not support platform specification" error when
  no Visual Studio C++ tools are installed. It checks for them before lifting
  and installs the Visual Studio 2022 Build Tools' C++ workload (several GB,
  after a Windows prompt) when they are missing.

### Known limits

- Japan 3.1: the Story ending runs at 8-11 fps, animation can look slightly
  off, the KO slowdown plays only when the computer's fighter is knocked out,
  the auto-save runs without the crash journal, and reverb sends are only
  logged.

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
