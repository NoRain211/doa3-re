Dead or Alive 3 native PC port - test build
===========================================

This is an experimental build for private testing. It contains no game
files. You build the game yourself from your own copy of Dead or Alive 3
(Xbox, USA). Other regions and versions are refused.

What you need (Windows 10 or 11, 64-bit):

- Python 3.12 or newer: https://www.python.org/downloads/windows/
  (tick "Add python.exe to PATH" or keep the "py" launcher).
- CMake 3.20 or newer: https://cmake.org/download/ (add it to PATH).
- Visual Studio 2022 or newer, or its Build Tools, with the
  "Desktop development with C++" workload:
  https://visualstudio.microsoft.com/downloads/
- An internet connection for the first build (it downloads SDL3 for
  controller support and the Capstone Python package).
- Your Dead or Alive 3 disc as an Xbox ISO, or as an extracted disc folder
  that contains default.xbe.

Build (once, about 10-20 minutes):

1. Extract this ZIP to a folder with a short path, for example C:\DOA3.
2. Drag your ISO, or the extracted disc folder, onto BuildGame.cmd.
3. Wait for "Build complete". The build is checked against the exact
   program the developers tested; any difference stops the build.

Play: double-click Launcher.cmd, pick resolution, anti-aliasing,
widescreen and volume, and press Play; it remembers your choices.
Widescreen widens the 3D view but stretches the HUD and menus. The first
boots fill the game's cache while the intro plays. Saves and the cache
live in private\play-disc.

Controls: the first connected controller (Xbox, PlayStation and other
controllers SDL3 supports). The keyboard drives player 1:

    Arrow keys = D-pad      Enter = START     Backspace = BACK
    A = X    S = Y    Z = A    X = B
    Q = White    W = Black    E = Left trigger    R = Right trigger

Known limits: the game shows 4:3 by default, as on the Xbox. The copyright
notice at boot runs fast on the first boot. Not every mode has been
played to its end; Story endings in particular are untested.

Reporting problems: say what you did and what happened, and send the
newest log from private\play-logs. If the build failed, send the logs
from the newest private\program-* folder.
