Dead or Alive 3 native PC port - Alpha (experimental)
=====================================================

This is an early, experimental build. It contains no game
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
2. Drag your ISO, or the extracted disc folder, onto Launcher.cmd or onto
   the open launcher window. Or open BUILD GAME, choose DOA3 Disc and pick
   your ISO, or default.xbe in the extracted disc folder.
3. Set Movies. They take 3.4 GB of the 4.5 GB disc; with Movies off the
   game skips every movie, including the intro. To get them back, build
   again from your ISO or disc folder with Movies on.
4. Choose Build and wait for "Build complete" in the information line.
   The build is checked against the exact program the developers tested;
   any difference stops the build. Closing the launcher asks before it
   stops a build.

Play: in Launcher.cmd, pick resolution, anti-aliasing, widescreen and
volume, then choose START GAME; it remembers your choices.
The launcher works with the mouse, the keyboard (arrows, Enter, Esc) or
an Xbox controller (D-pad or left stick, A to select, B to go back).
Widescreen widens the 3D view; the HUD and menus stay 4:3 in the center. The first
boots fill the game's cache while the intro plays. Saves and the cache
live in private\play-disc.

Controls: the first connected controller (Xbox, PlayStation and other
controllers SDL3 supports). The keyboard drives player 1:

    Arrow keys = D-pad      Enter = START     Backspace = BACK
    A = X    S = Y    Z = A    X = B
    Q = White    W = Black    E = Left trigger    R = Right trigger

The D-pad and the left stick move the fighter. The game's Analog Button
setting is turned on at every boot, since the game reads the stick only
with it on.

DOAXBV swimsuits (optional): every DOA3 woman can wear her swimsuits from
Dead or Alive Xtreme Beach Volleyball, built from your own DOAXBV disc.
No DOAXBV data ships with this ZIP.

1. Put your DOAXBV disc in this folder's private folder: the .iso (with
   "xtreme", "doax" or "beach" in its name), or its extracted folder, the
   one holding datahdd.afs and datadvd.afs. Or drop either on the launcher
   window, or choose it under SWIMSUITS > DOAXBV Disc.
2. Leave SWIMSUITS > Swimsuits On (the default) and choose START GAME.
3. In a fight, click L3 (press in the left stick) to open the swimsuit
   menu, and click it again to close it. While it is open:

       LB / LT = P1's next / previous swimsuit (hold to scroll)
       RB / RT = P2's next / previous swimsuit
       X / B   = mark P1's / P2's swimsuit as a favorite
       Y       = browse only favorites, or every swimsuit again

   The game ignores that controller while the menu is open. On the
   keyboard, 1 / 2 change P1's / P2's swimsuit (Shift goes back).

If L3 does nothing, the disc was not found: the newest log in
private\play-logs has no "suits in" line. SWIMSUITS > Start in a favorite
starts each fighter in her first favorite. More in
docs\doaxbv-swimsuits.md.

Known limits: the game shows 4:3 by default, as on the Xbox. The copyright
notice at boot runs fast on the first boot. Analog Button is turned on at
every boot, so turning it off does not last. Not every mode has been
played to its end; Story endings in particular are untested.

Reporting problems: open an issue at
https://github.com/NoRain211/doa3-re/issues and say what you did and what
happened. Copy the "recomp stop" or crash lines from the end of the newest
log in private\play-logs, or the error from the newest private\program-*
folder if the build failed. Remove your user name and other private paths,
and do not upload whole logs or any game files.
