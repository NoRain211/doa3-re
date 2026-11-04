# Rollback netcode

Status: two players can fight online. Pressing A on Versus in the main menu
opens the netplay menu, where one player hosts a room and the other joins it
by address or finds it on FIND MATCH. When both are ready the game boots
again in the same window, walks both players to Versus character select and
plays the session. Menus run in lockstep and live rounds roll back. Every
session records a replay. Results below come from the Japan 3.1 build unless
noted. The USA build has played a 4,000-frame command-line session with
identical guest RAM on both runners; its menu sessions have not run yet.

The rollback ring, transport, lobby, room, matchmaker client, replays and
menu are DOA2U's, unchanged except where this page says so. Their design,
packet formats and pacing are described in
[doa2u-re](https://github.com/NoRain211/doa2u-re)'s `docs/rollback-netcode.md`
and `docs/matchmaking.md`, on its `codex/rollback-netcode` branch.
DOA3's packets and replay headers keep DOA2U's magic numbers. The build
digest in every lobby, room and replay header keeps the two games apart:
both use DOA2U's matchmaker, whose room lists only match runners of the
same build, and a replay only plays on the build that recorded it.

## Determinism

DOA3 already ran on a deterministic clock for input logs: guest time is a
counter that advances only when the game waits, by exactly the time it
waits for, and the system clock starts at a fixed date. Sessions, replays
and SyncTest use it too, so the CRT's `srand(time(0))` reseeds at boot,
match start and stage load give both players the same numbers. File times
read as that date as well.

The frame boundary is `D3DDevice_Present` and `D3DDevice_Swap` returning
to the guest. SyncTest (`RECOMP_SYNCTEST`) saves every runtime module and
the written guest RAM pages there, runs frames again and compares them. It
ignores kernel data (`0x00f80000` to `0x01000000`) and the fiber, thread
and kernel stacks, which the host writes on its own schedule. In the Japan
build, from swap 2500 every 8 frames through a scripted Story fight, the
title and attract mode, it matched 831 times up to swap 9216. Its only skip
was the frame after the game's save, which a rewind must not cross.

Frames run again present nothing and play no sound; the D3D frame adapter
drops their flips, clears and draws, and DirectSound drops their output,
while the models advance as usual. The runner computes RCPSS and RSQRTSS
exactly (`RECOMP_PORTABLE_MATH`), as DOA2U does.

## Input

The game reads pads only through its XInput poll (`0x9EAF0`, Japan
`0xA4DD0`), so the input adapter's ports 0 and 1 carry the session's two
players and ports 3 and 4 stay empty. Both runners report the same
controllers, which the Japan build also checks by subtype. The fight maps
players to ports through `0x2FD550` (Japan `0x30E3F4`).

## Live rounds

A session predicts only while a round is live (`game_state.c`): game
state 1, the pause menu closed, the fight tasks running, round phase 1, and
the round neither decided nor out of time. Elsewhere it runs in lockstep, so
saves after a match never run on a guess.

## Walk to Versus

A session starts from boot. Both sides replace port 0's pad with Start until
the front end runs, Right until the main menu has Versus (index 5) under its
cursor, then A, and A again for Single on the battle type menu, until
character select settles. Each press lasts one frame in every 16 and waits
for the scene to settle. The walk is hidden and silent behind the STARTING
MATCH panel. In the scripted runs it reached character select at frame
597. After a room session, port 0 walks back to the main menu with Versus
chosen while the reopened room covers the screen.

## The game's cache

DOA3 copies its three archives from the disc to the Z: utility drive over
its first boots. A player whose copy had not finished desynced at frame 30,
because their boot ran the copy. Before a session connects, the runner
copies any missing or partial archive into the play folder's
`.recomp-storage/partition5` itself, as the game would. They are copies,
not hardlinks, so a copy the game starts again never writes through to the
disc's files.

A player who had never booted the game also desynced: their boot formatted
the cache partition. A session therefore keeps the utility drives' format
sectors (`partition0` and `partition*.raw`) in its own
`.recomp-storage/netplay` folder, which is wiped when the session starts,
so both sides format alike. The cached archives stay shared.

## Screen shape

`RECOMP_D3D_WIDESCREEN` reaches the guest: it sets the video flags the
game reads and the camera constants the runner writes. A 16:9 host and a
4:3 joiner desynced at frame 30. The netplay build identity therefore
includes the shape (reduced, so 1920:1080 is 16:9), and runners with
different shapes refuse each other like different builds.

## Booting again

A room's session and the return from it boot the game again in the same
process and window. The runner maps the XBE into fresh guest RAM and runs
its per-boot setup again (`bootGuest`) on a new native fiber, after
`recomp_reset_runtime_modules` puts every module back to power-on: files,
DirectSound, the soundtrack, CRI, the D3D models (the presenter and its
window stay), threads, fibers, kernel memory, interrupts, video, the heap,
input, the clock, the boot fast-forward and the Analog Button fix.

## Results

All on one machine, each runner in its own hardlinked play folder, from the
Japan build's scripted input:

| Run | Result |
|---|---|
| Loopback, generated remote pads 4 frames late, 600 frames from a Story fight | 594 rollbacks, 2,376 frames run again; guest RAM identical to lag 0 |
| `--netplay-host` and `--netplay-join`, input delay 0, 4,000 frames | Versus character select at frame 597, rollback in both live windows, guest RAM identical; the joiner's replay matched it |
| Room session from the menu, 3,500 frames | Guest RAM identical on both sides and to the joiner's replay played in a fresh process |
| Host kept through a killed joiner, the return to its room and a second session with a new joiner | Back at the Versus menu after the 15 s timeout; guest RAM identical at frame 4,000 |
| FIND MATCH through a local matchmaker started with `--relay-only` | Joined through the relay; guest RAM identical at frame 2,500 |

A frame run again costs about 5 ms with one frame of rollback and 7.4 ms at
eight, against 3.7 ms in DOA2U, so two or three frames of rollback fit in a
60 Hz frame. The room suggests an input delay of half the round trip, which
leaves rollback for jitter.

## Limits

- Only the Japan 3.1 build has played sessions from the netplay menu; the USA
  build has played command-line sessions only.
- DOA3 has no profile names, so every player shows as PLAYER.
- The clipboard works only on Windows. macOS and Linux forward typed text
  through SDL, but no build there has tried it.
- IPv4 only, as in DOA2U.
- Tag Battle and other modes than Versus Single are unverified.
