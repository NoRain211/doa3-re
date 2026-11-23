# Europe 3.2 (work in progress)

Bring-up of the European disc (Europe, Australia; En, Ja, Fr, De, Es, It),
default.xbe SHA-256
2d1b63b072fdefcd97174afeb28a111ce9443633f3f2a12122a8dea3fca037ab, title ID
0x4D53002D, region mask 4, built 2002-02-14. Every statically linked library
(XAPILIB, D3D8, D3DX8, XGRAPHC, DSOUND, XBOXKRNL, LIBCMT, LIBCPMT) is XDK
4134, the same as Japan 3.1, so the Japan recipe is the starting point, not
USA.

## Step 1: map Japan to Europe without a Japan disc

The Europe disc is the only input. The Japan recipe ships function bounds
(tools/doa3-jp/functions.json.gz) but no bytes, so the mapping uses
function-size runs instead of Version Tracking:

- tools.disasm on the Europe XBE: 6,609 functions.
- Per section, align the Japan and Europe size sequences and keep runs of
  three or more equal sizes. 6,384 of 6,641 Japan functions (96.1 %) land in
  such runs.
- Library sections move as one block: D3D, D3DX, XGRPH, DSOUND, PSGSFD and
  XPP are all Japan + 0x940. Game .text moves in many blocks with their own
  shifts (+ 0x970 is the largest, 1,659 functions; the constants in this
  change also use + 0x2A0, + 0x150, + 0xF0 and + 0xC0, among others), so a
  game-code address always comes from the function map, never a fixed shift.
- All 79 recipe addresses (manual_functions.json plus program_forwards_jp.c)
  map: 76 to function starts, 3 to interior points. 71 move + 0x940 and 8
  move + 0x970.
- Spot checks: D3DDevice_Swap (Japan 0x001E14D0) is Europe 0x001E1E10 and
  ends ret 4; IDirectSoundBuffer_SetHeadroom (Japan 0x001F1DA5) is Europe
  0x001F26E5 and ends ret 8, as on Japan.
- The XAPI TLS index is Europe 0x0040ABB8 (XBE header), Japan 0x0040A178:
  + 0xA40 for .data.

Data constants: code addresses follow the function map. Data moves in three
ranges, by Japan address:

- up to at least 0x003049B8 (camera floats at 0x00251xxx, the menu table at
  0x0029CB20, the character byte at 0x002FF868, the sprite records): + 0x940;
- from at most 0x003F4563 (the options defaults byte, the XAPI TLS index, the
  ADXM globals at 0x0040C6F4) through the fight data (0x005B1424): + 0xA40;
- from 0x0073C400 up: - 0x44FA0.

The step from + 0x940 to + 0xA40 lies between 0x003049B8 and 0x003F4563;
without the Japan disc its exact place is not measured, so a new constant in
that range needs checking against the addresses Europe's code reads. Three reads in Europe code
fixed that shift: the vertical-blank thread resumes the ADXM main thread from
0x00C3B0D8 (Japan 0x00C80078), the scheduler walks its task table at
0x006F7460 (Japan 0x0073C400), and the select-next yield reads its slots at
0x00972970 (Japan 0x009B7910). 11 of the 12 constants in that range hit an
address the Europe code references; MENU_CURSOR is only reached by offset.

## Build

DOA3(usa, japan, europe) picks a guest address; DOA3_4134(japan, europe) does
the same in code only the 4134 builds compile. RECOMP_DOA3_EUR implies
RECOMP_DOA3_JP, so Europe takes every Japan binding. program_forwards_eur.c
keys the table by Europe address and keeps the USA adapter keys. The two
Japan-only adapters stay keyed as on Japan: Swap at its Japan address, which
the table translates, and SetHeadroom at its Europe address.

## Results

Boot | Change | Result
--- | --- | ---
1 | first lift, NVIDIA Vulkan, offscreen SDL | host crash in the driver's swapchain query, after CreateDevice and the first Clear
2 | lavapipe | first frame presented, threads and fibers run; stops at unbound SetHeadroom
3 | SetHeadroom keyed at its Europe address, 180 s | no stop, 7,028 presents, black frames; the Z: cache fill stops after 5.4 MB of the voice archive, no audio buffers
4 | unpaced | lavapipe fails a descriptor allocation (host)
5 | bss constants moved - 0x44FA0, empty cache, 240 s | no stop: opening movie, Team NINJA logo, title screen, textured 3D attract stages; voice archive cached in full, music archive 105 of 229 MB; no audio buffers with output muted
6 | Steam Deck, Vulkan on RADV, same runner | opening movie, title, mode and character select, Story fights (Kasumi v Hitomi, Hitomi v Bayman) with damage and pause; non-silent audio; menus driven by a virtual Xbox pad; three clean exits. Story's infinite timer is the retail setting (bring-up.md)
7 | Steam Deck, build_steamdeck.sh from the ISO | builds and boots; options survive a clean exit and relaunch (15,432-byte save); Time Attack counts down and shows results; a Survival win; a Tag win and its second stage; Story to the fourth opponent
8 | KO blend setup and Quit Game help texts at Europe addresses | Time Attack KOs, replay and next opponent; Quit Game absent (below)
9 | memmove's slot-0 tail (below) | Options, Video Settings, Regulator opens and adjusts; a sparring fight after it

Boots 1 to 5 run headless on a host without a display, so they use lavapipe;
NVIDIA's driver crashes on the offscreen surface.

## memmove's slot-0 tail

The CRT memmove copying down with no dwords left jumps through its trail
table at its base, slot 0, which holds 0x001B8017. The lifter reads that
table from the slot below its base and leaves slot 0 out, so the jump fell
through to an unresolved indirect call and the game exited (the video
settings' Regulator screen reached it). Boot 9 handled that address by hand.
xboxrecomp 4fe8a59 keeps the slot at a downward table's base, so the lift now
holds the arm and the hand-written copy is gone.

## Quit Game

The main menu's Quit Game entry (menu_quit.c) is off on Europe: the install
logs that and returns, so every menu hook passes the game through. Europe's
entry table holds 10-byte entries (mode, then three sprite ids and a help
number) instead of Japan's 12, and its sprite records are renumbered: the
Japan ids fail the install's size checks (0x3C4 is 255x18, 0x188 36x36).
Japan + 0x11 gives sprites of the right sizes (0x3D5 128x128, 0x199 128x24);
the words texture and the help lines are not mapped yet.

## Next

- Quit Game on Europe's menu layout.
- The 5 game seams (boundary and collision passes, save-journal auto-save) on
  Europe code.
- Story past the fourth opponent and its ending; pacing measurements.
