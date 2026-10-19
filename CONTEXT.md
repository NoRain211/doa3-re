# DOA3 Native Port

This context names the product and custody boundaries of the native PC port.
It keeps public planning precise without exposing locally derived game content.

## Product boundary

**Host shell**:
The distributable, game-independent Windows, macOS or Linux executable that
provides the native runtime and loads a locally prepared game payload.
_Avoid_: Complete game binary, emulator

**Game import**:
The local transformation of a user-owned verified ISO or XISO into the private
inputs consumed by the host shell.
_Avoid_: Game download, bundled installer

**Private game payload**:
The locally derived game logic and data consumed by the host shell, including
generated modules and extracted assets that must never enter the public tree.
_Avoid_: Public game assets, redistributable payload

**Cross-game content**:
Content from another game the player owns, rebuilt on the player's machine from
their own disc of that game.
_Avoid_: Bundled DOAXBV assets, downloadable content pack

**Verified disc revision**:
The one explicitly authenticated retail disc identity supported by the first
feature-complete target.
_Avoid_: Any valid disc, generic retail build

## Completion boundary

**Feature complete**:
Every offline mode, activity, media path, and save flow on the verified disc
revision, including local multiplayer, is usable through the host shell.
_Avoid_: Fully decompiled, cycle accurate

**Local multiplayer**:
Play by two or more people, each on their own controller, attached to one host
shell.
_Avoid_: Netplay, online play, system link

**Functional parity**:
The scoped game produces equivalent player-observable outcomes while documented
presentation and timing differences remain acceptable.
_Avoid_: Byte matching, pixel matching

**Presentation modernization**:
The supported modern display and rendering behavior layered over functional
parity, including high-resolution, widescreen, high-refresh, and visual options.
_Avoid_: Accuracy mode, hardware emulation

## Runtime boundary

**Shared runtime**:
The game-independent native runtime that this port and its sister DOAXBV and
DOA2U ports all consume; each game keeps its own bindings to it.
_Avoid_: Copied runtime, runtime fork
