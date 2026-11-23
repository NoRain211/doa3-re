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
[doa2u-re](https://github.com/NoRain211/doa2u-re)'s `docs/rollback-netcode.md`,
on its `codex/rollback-netcode` branch. The matchmaker server is DOA2U's
room-protocol server, kept here as `tools/matchmaker.py` since DOA2U's own
moved to Xbox Live; [matchmaking.md](matchmaking.md) describes it and its
packets. DOA3's packets and replay headers keep DOA2U's magic numbers. The
build digest in every lobby, room and replay header keeps the two games
apart: the matchmaker lists rooms only to runners of the same build, and a
replay only plays on the build that recorded it.

The client also learns an IPv6 path through the matchmaker, over a second,
IPv6-only socket that sends its address (ADDR6) with every ask. When both
players have one, rooms punch IPv4 and IPv6 together and a peer that answers
over IPv6 is sent to there, though a relay fallback overrides it; IPv4 stays
primary. It needs the matchmaker to listen on IPv6 with an AAAA record; see
[matchmaking.md](matchmaking.md#ipv6).

DOA3 has no player profile, so NETPLAY SETTINGS has a NAME of up to 15
letters, digits and spaces, checked with the same banned-name filter as
DOA2U's profile names. A player with no name who opens HOST ONLINE, JOIN
ONLINE or FIND MATCH is asked for one, once per launch; B skips it and
they show as PLAYER. Rooms and FIND MATCH show the name. In a room's
session the runner draws both names above the life bars from the intro
to the winner's pose (game state 1 or 3), this side's in yellow, as DOA2U
draws profile names where Xbox Live drew gamertags. It draws over the
frame and writes no guest memory, so the desync hash is unchanged.
Command-line sessions and replays have no names.

B leaves a room, and the runner first tells the other player with its own
leave message, separate from the session's goodbye, which can arrive late
into a room both players just returned to. A host that hears it says who
left and lets the next player in; a joiner drops the host as its peer and
shows THE HOST CLOSED THE ROOM, so a room opened again on the same port
starts empty.

## Determinism

DOA3 already ran on a deterministic clock for input logs: guest time is a
counter that advances only when the game waits, by exactly the time it
waits for, and the system clock starts at a fixed date. Sessions, replays
and SyncTest use it too, so the CRT's `srand(time(0))` reseeds at boot,
match start and stage load give both players the same numbers. File times
read as that date as well, and the game sees only whether a file is a
directory or read-only in its attributes: Windows adds archive and other
bits that Linux and macOS never report.

The frame boundary is `D3DDevice_Present` and `D3DDevice_Swap` returning
to the guest. SyncTest (`RECOMP_SYNCTEST`) saves every runtime module and
the written guest RAM pages there, runs frames again and compares them. It
ignores kernel data (`0x00f80000` to `0x01000000`) and the fiber, thread
and kernel stacks, which the host writes on its own schedule. In the Japan
build, from swap 2500 every 8 frames through a scripted Story fight, the
title and attract mode, it matched 831 times up to swap 9216. Its only skip
was the frame after the game's save, which a rewind must not cross.
In the USA build, every 8 frames from swap 2500, attract mode matched
8,001 times up to swap 66,508 and a scripted Story route 4,134 times up to
swap 35,640, again skipping only the frame after a save. Before host output
left the saved state, a 3D voice's reverb send position differed after
frames run again (swap 4,060).

A save reads the list of written guest RAM pages without clearing it and
compares each with a shadow copy, clearing the list only once it holds more
than twice the changed pages plus 64; after a clear the first write to every
page faults. A rewind writes each page once, from the oldest save it undoes.
Saves clear only the fiber states' headers, as a restore reads just the used
stack bytes. Page hashes are brought up to date at a check, not at every save.

Frames run again present nothing and play no sound; the D3D frame adapter
drops their flips, clears and draws, and DirectSound drops their output,
while the models advance as usual. A dropped draw is neither deferred nor
saved, so a hidden frame builds no draw at all. Host output keeps its own
real-time place in each buffer outside the saved state, so a rewind neither
replays nor loses what was heard; a re-simulated frame neither plays nor
moves it. The runner computes RCPSS and RSQRTSS exactly
(`RECOMP_PORTABLE_MATH`), as DOA2U does.

## Input

The game reads pads only through its XInput poll (`0x9EAF0`, Japan
`0xA4DD0`), so the input adapter's ports 0 and 1 carry the session's two
players and ports 2 and 3 stay empty. Both runners report the same
controllers, which the Japan build also checks by subtype. The fight maps
players to ports through `0x2FD550` (Japan `0x30E3F4`).

A local pad is stored with the values DOA3's pad conversion (`0x9EB90`,
Japan `0xA4E70`) reads as zero set to zero: sticks within 0x2000 of center,
face, Black and White under 0x20, and triggers under 0x80. The fight reads
only its zeroed stick copy, and a trigger's raw copy only while that trigger
is pressed (`0x21110`), so a resting stick's noise is no misprediction.

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

Backing out of character select to the main menu ends the session
(`netplay:left-versus`): a room session goes back to its room, and a
command-line one stops with exit code 4. Menus run in lockstep, so both
sides see the settled main menu at the start of the same frame and leave
together.

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
4:3 joiner desynced at frame 30. Sessions and replays therefore boot the game
at 16:9 on every screen, and an offline boot gets the player's own shape back
(`chooseScreenShape` in `runner_boot.cpp`). When a reboot changes the shape,
the presenter rebuilds its main targets on the same window if their size
changes (above scale 1) and fits the picture with black bars, so a 4:3 or 21:9 player sees 16:9 online. The shape
is no longer part of the netplay build identity, so players on any screen
find each other's rooms. A 16:9 host and a joiner set to 4:3, both USA 3.0
from the command line, had identical guest RAM at frame 2,500.

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
Japan build's scripted input. Runs compared with each other need play folders
whose `.recomp-storage` starts alike, or the game loads different saves:

| Run | Result |
|---|---|
| Loopback from boot, generated remote pads 8 frames late, 4,000 frames | 3,980 rollbacks, 31,840 frames run again; guest RAM identical to lag 0 (also at lag 4 over 1,500 frames) |
| `--netplay-host` and `--netplay-join`, input delay 0, 4,000 frames | Versus character select at frame 597, rollback in both live windows, guest RAM identical; the joiner's replay matched it |
| Room session from the menu, 3,500 frames | Guest RAM identical on both sides and to the joiner's replay played in a fresh process |
| Host kept through a killed joiner, the return to its room and a second session with a new joiner | Back at the Versus menu after the 15 s timeout; guest RAM identical at frame 4,000 |
| FIND MATCH through a local matchmaker started with `--relay-only` | Joined through the relay; guest RAM identical at frame 2,500 |
| `--netplay-host` and `--netplay-join`, the host pressing B twice from character select | Both sides left Versus at frame 984; with one B, back on the battle type menu, both ran to frame 3,000 |

Each frame run again costs about 5 ms with one frame of rollback and 7.4 ms
at eight, against 3.7 ms in DOA2U, so about two frames of rollback fit beside
the live frame in a 60 Hz frame. The room suggests an input delay of half
the round trip, which leaves rollback for jitter.

Shorter hitches in a live round are logged once per second when the second
had one: the time this side waited for the other player's pads after its
frame was due, its own slowest frame, how many frames it ran again after
rewinds and how long they took, and frames frame sync held back so the
other side could catch up. A slow second with long waits points at the other
machine; a slow frame here points at this one, and it includes any frames run
again after a rewind, since the player sees them as one late frame. The
summary line at the end of a run reports the time spent loading rewinds,
the total frames run again and their time, and the time spent saving. Both
renderers log each draw shader they build outside their boot precompile,
with how long it took.

The snapshot, pad, hidden-draw and timing changes above are ported from
DOA2U (`ae35d38`, `1dae1ee` and `e5fe8f0`) and have not been measured in
DOA3; the timings on this page predate them.

## Limits

- Only the Japan 3.1 build has played sessions from the netplay menu; the USA
  build has played command-line sessions only.
- macOS and Linux rewind native fibers as Windows does, but no session or
  SyncTest run of the game has played there yet.
- The fight names sit at fixed spots measured on the 4:3 HUD.
- The clipboard works only on Windows. macOS and Linux forward typed text
  through SDL, but no build there has tried it.
- The matchmaker is reached over IPv4; IPv6 is only an extra path between
  matched players.
- Tag Battle and other modes than Versus Single are unverified.
