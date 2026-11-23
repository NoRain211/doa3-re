# SPDX-License-Identifier: GPL-3.0-or-later
"""Play DOA3 on the local build: the macOS and Linux counterpart of tools/play.ps1."""
import argparse
from datetime import datetime
import json
import os
from pathlib import Path
import shutil
import string
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
# Pad buttons and keys that private/keyboard.json may bind; the codes are Windows virtual keys.
# F10 is left out because Windows treats it as a system key, and F11 is reserved.
KEY_ACTIONS = ("up", "down", "left", "right", "start", "back", "a", "b", "x", "y", "white", "black", "ltrig", "rtrig", "l3")
KEY_CODES = {**{c: ord(c) for c in string.ascii_uppercase + string.digits},
             **{f"F{n}": 0x6f + n for n in range(1, 13) if n not in (10, 11)},
             "BACKSPACE": 0x08, "TAB": 0x09, "ENTER": 0x0d, "SHIFT": 0x10, "CTRL": 0x11, "ESC": 0x1b,
             "SPACE": 0x20, "LEFT": 0x25, "UP": 0x26, "RIGHT": 0x27, "DOWN": 0x28}


def keyboard_map():
    """RECOMP_KEYMAP text for private/keyboard.json, e.g. {"a": "Space"}; "" if absent."""
    try:
        bindings = json.loads((ROOT / "private/keyboard.json").read_text(encoding="utf-8"))
    except FileNotFoundError:
        return ""
    except (OSError, ValueError) as error:
        raise ValueError(f"private/keyboard.json is not valid JSON: {error}")
    if not isinstance(bindings, dict):
        raise ValueError("private/keyboard.json must be an object such as {\"a\": \"Space\"}")
    for action, key in bindings.items():
        if action not in KEY_ACTIONS or not isinstance(key, str) or key.upper() not in KEY_CODES:
            raise ValueError(f"private/keyboard.json: cannot bind {action!r} to {key!r}; buttons are "
                             f"{', '.join(KEY_ACTIONS)}; keys are A-Z, 0-9, F1-F9, F12, Up, Down, Left, Right, "
                             "Enter, Backspace, Space, Tab, Shift, Ctrl and Esc")
    return ",".join(f"{action}={KEY_CODES[key.upper()]:x}" for action, key in bindings.items())


def runner(version):
    """The runner of an installed version, which lives in that version's own build folder."""
    name = "recomp_program_runner.exe" if sys.platform == "win32" else "recomp_program_runner"
    return ROOT / version["build_dir"] / name


def installed_versions():
    """private/build.json's installed versions, most recently built last. A version
    is installed while its runner and its play folder exist; a build older than
    the record's installed list is USA 3.0 in play-disc and build/recomp-program."""
    try:
        record = json.loads((ROOT / "private" / "build.json").read_text(encoding="utf-8"))
    except (OSError, ValueError):
        record = {}
    if not isinstance(record, dict):
        record = {}
    listed = record.get("installed")
    entries = ([entry for entry in listed if isinstance(entry, dict)] if isinstance(listed, list)
               else [{"version": record.get("version", "USA 3.0"), "play_disc": record.get("play_disc", "play-disc"),
                      "xbe": record.get("xbe", "default.xbe"), "build_dir": "build/recomp-program"}])
    versions = [{"version": entry.get("version") or "USA 3.0",
                 "play_disc": entry.get("play_disc") or "play-disc",
                 "xbe": entry.get("xbe") or "default.xbe",
                 "build_dir": entry.get("build_dir") or "build/recomp-program"} for entry in entries]
    return [each for each in versions
            if runner(each).is_file() and (ROOT / "private" / each["play_disc"]).is_dir()]


def chosen_version(name=None):
    """The version to play: the one named, else the one private/launcher.json
    remembers while it is installed, else the most recently built."""
    installed = installed_versions()
    if name is not None:
        return next((each for each in installed if each["version"] == name), None)
    if not installed:
        return None
    try:
        name = json.loads((ROOT / "private" / "launcher.json").read_text(encoding="utf-8")).get("version")
    except (OSError, ValueError, AttributeError):
        name = None
    return next((each for each in installed if each["version"] == name), installed[-1])


def play_disc(version):
    """A hard-linked view of the extracted disc keeps the game's cache and saves
    (.recomp-storage) out of the source disc. The first boots fill the cache."""
    path_file = ROOT / "private" / "disc-path.txt"
    source = (Path(path_file.read_text(encoding="utf-8").splitlines()[0].strip())
              if path_file.is_file() else ROOT / "private" / "imported-disc" / "disc")
    source = source.resolve(strict=True)
    disc = ROOT / "private" / version["play_disc"]
    if disc.exists():
        return disc
    # Fill a staging folder so an interrupted copy is never taken as complete.
    staging = disc.with_name(disc.name + ".tmp")
    if staging.exists():
        shutil.rmtree(staging)
    for file in source.rglob("*"):
        relative = file.relative_to(source)
        if not file.is_file() or relative.parts[0] == ".recomp-storage":
            continue
        target = staging / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        try:
            os.link(file, target)
        except OSError:
            shutil.copy2(file, target)
    staging.rename(disc)
    return disc


def doaxbv_disc():
    """The player's DOAXBV disc for the swimsuit switch: an .iso image or a
    folder holding datahdd.afs and datadvd.afs. From RECOMP_DOAXBV_DISC, then
    private/doaxbv-path.txt (the launchers' "Choose DOAXBV disc"), then a
    DOAXBV image or extracted folder placed in private/, then the disc of a
    doaxbv-re checkout beside this one; a found disc is remembered."""
    named = os.environ.get("RECOMP_DOAXBV_DISC")
    if named:
        return Path(named)
    path_file = ROOT / "private" / "doaxbv-path.txt"
    if path_file.is_file():
        lines = path_file.read_text(encoding="utf-8").splitlines()
        if lines and Path(lines[0].strip()).exists():
            return Path(lines[0].strip())
    for image in sorted((ROOT / "private").glob("*.iso")):
        name = image.name.lower()
        if "xtreme" in name or "doax" in name or "beach" in name:
            return image
    sister = ROOT.parent / "doaxbv-re" / "private" / "disc"
    for afs in [*sorted((ROOT / "private").glob("*/datahdd.afs")), *sorted((ROOT / "private").glob("*/*/datahdd.afs")),
                sister / "datahdd.afs", *sorted(sister.glob("*/datahdd.afs"))]:
        if afs.is_file() and (afs.parent / "datadvd.afs").is_file():
            path_file.parent.mkdir(parents=True, exist_ok=True)
            path_file.write_text(str(afs.parent), encoding="utf-8")
            return afs.parent
    return None


def suit_pack():
    """The suit recipes ship as recipes/doaxbv-suits.pack.gz (a copy in
    private/ is used only when that is missing); the runtime reads them
    unpacked, so unpack once into private/."""
    pack = ROOT / "private" / "doaxbv-suits.pack"
    packed = next((p for p in (ROOT / "recipes" / "doaxbv-suits.pack.gz",
                               ROOT / "private" / "doaxbv-suits.pack.gz") if p.is_file()),
                  ROOT / "recipes" / "doaxbv-suits.pack.gz")
    if packed.is_file() and (not pack.is_file() or pack.stat().st_mtime < packed.stat().st_mtime):
        import gzip
        pack.parent.mkdir(parents=True, exist_ok=True)
        staging = pack.with_name(pack.name + ".tmp")
        with gzip.open(packed, "rb") as source, staging.open("wb") as target:
            shutil.copyfileobj(source, target)
        staging.replace(pack)
    return pack if pack.is_file() else None


def swimsuit_env():
    """Variables for the swimsuit switch: every DOA3 woman can wear her DOAXBV
    swimsuits, built from the player's own DOAXBV disc (see README)."""
    disc, pack = doaxbv_disc(), suit_pack()
    if disc is None or pack is None:
        return {}
    env = {"RECOMP_DOAXBV_DISC": str(disc), "RECOMP_COSTUME_PACK": str(pack)}
    try:
        if json.loads((ROOT / "private" / "launcher.json").read_text(encoding="utf-8")).get("swimsuit_start_favorite"):
            env["RECOMP_COSTUME_START_FAVORITE"] = "1"
    except (OSError, ValueError, AttributeError):
        pass
    return env


def swimsuits_enabled():
    """On unless the launcher turned it off."""
    try:
        return bool(json.loads((ROOT / "private" / "launcher.json").read_text(encoding="utf-8")).get("swimsuits", True))
    except (OSError, ValueError, AttributeError):
        return True


def launcher_settings():
    """The choices launcher.sh saved, as the variables Launcher.exe sets."""
    try:
        settings = json.loads((ROOT / "private" / "launcher.json").read_text(encoding="utf-8"))
        # Settings saved before 16:10 hold a widescreen flag.
        screen = settings.get("screen") or ("16:9" if settings.get("widescreen") else "4:3")
        return {"RECOMP_D3D_SCALE": f"{int(settings.get('height', 480)) / 480:.4f}",
                "RECOMP_D3D_MSAA": str(int(settings.get("msaa", 1))),
                "RECOMP_D3D_WIDESCREEN": "0" if screen == "4:3" else str(screen),
                "RECOMP_AUDIO_GAIN": f"{int(settings.get('volume', 100)) / 100:.4f}",
                "RECOMP_FREE_CAMERA": "1" if settings.get("free_camera") else "0"}
    except (OSError, ValueError, TypeError, AttributeError, OverflowError):
        return {}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--launcher", action="store_true",
                        help="Pick settings first (Linux; see launcher.sh)")
    parser.add_argument("--vsync", action="store_true",
                        help="Also synchronize presentation to the display")
    parser.add_argument("--version", metavar="NAME",
                        help='Play this installed version, such as "Japan 3.1", instead of the remembered one')
    args = parser.parse_args()
    version = chosen_version(args.version)
    if version is None:
        if args.version:
            installed = [each["version"] for each in installed_versions()]
            print(f"Dead or Alive 3 {args.version} is not installed. Installed: "
                  + (", ".join(installed) if installed else "nothing yet"), file=sys.stderr)
        else:
            print("The game is not built yet. Run ./build_game.sh with your ISO first.", file=sys.stderr)
        return 1
    runner_path = runner(version)
    if args.launcher:
        # Linux builds the settings launcher beside the runner.
        launcher = runner_path.parent / "recomp_launcher"
        if not launcher.is_file():
            print(f"{launcher} is missing. Rebuild with ./build_game.sh, "
                  "or start the game with ./run_game.sh.", file=sys.stderr)
            return 1
        if subprocess.run([str(launcher), str(ROOT)]).returncode != 0:
            return 1
        # The launcher saves the picked version and can build another one.
        version = chosen_version(args.version)
        if version is None:
            print("The game is not built yet. Run ./build_game.sh with your ISO first.", file=sys.stderr)
            return 1
        runner_path = runner(version)
    disc = play_disc(version)
    env = dict(os.environ)
    # Test-harness settings would speed the game up or stop it after a timeout.
    env.pop("RECOMP_UNPACED", None)
    env.pop("RECOMP_WATCHDOG_MS", None)
    for name, value in launcher_settings().items():
        env.setdefault(name, value)
    try:
        keymap = keyboard_map()
    except ValueError as error:
        print(error, file=sys.stderr)
        return 1
    if keymap:
        env.setdefault("RECOMP_KEYMAP", keymap)
    if swimsuits_enabled():
        for name, value in swimsuit_env().items():
            env.setdefault(name, value)
    logs = ROOT / "private" / "play-logs"
    logs.mkdir(parents=True, exist_ok=True)
    # The PID keeps two launches in the same second from colliding.
    log = logs / (datetime.now().strftime("%Y%m%d-%H%M%S") + f"-{os.getpid()}.log")
    runner_args = [str(runner_path), "--xbe", str(disc / version["xbe"])]
    if args.vsync:
        runner_args.append("--vsync")
    with log.open("x") as err, log.with_name(log.name + ".out").open("x") as out:
        result = subprocess.run(runner_args, cwd=ROOT, env=env, stdout=out, stderr=err)
    print(f"exit={result.returncode} log={log}")
    return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
