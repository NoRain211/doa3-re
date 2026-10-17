# SPDX-License-Identifier: GPL-3.0-or-later
"""Play DOA3 on the local build: the macOS and Linux counterpart of tools/play.ps1."""
import argparse
from datetime import datetime
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
RUNNER = ROOT / "build" / "recomp-program" / "recomp_program_runner"


def play_disc():
    """A hard-linked view of the extracted disc keeps the game's cache and saves
    (.recomp-storage) out of the source disc. The first boots fill the cache."""
    path_file = ROOT / "private" / "disc-path.txt"
    source = (Path(path_file.read_text(encoding="utf-8").splitlines()[0].strip())
              if path_file.is_file() else ROOT / "private" / "imported-disc" / "disc")
    source = source.resolve(strict=True)
    disc = ROOT / "private" / "play-disc"
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
    DOAXBV image placed in private/."""
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
    return None


def suit_pack():
    """The suit recipes come as doaxbv-suits.pack.gz (a release download, see
    docs/doaxbv-swimsuits.md) in private/ or recipes/; the runtime reads them
    unpacked, so unpack once into private/."""
    pack = ROOT / "private" / "doaxbv-suits.pack"
    packed = next((p for p in (ROOT / "private" / "doaxbv-suits.pack.gz",
                               ROOT / "recipes" / "doaxbv-suits.pack.gz") if p.is_file()),
                  ROOT / "private" / "doaxbv-suits.pack.gz")
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
    return {"RECOMP_DOAXBV_DISC": str(disc), "RECOMP_COSTUME_PACK": str(pack)}


def swimsuits_enabled():
    """On unless the launcher turned it off."""
    try:
        return bool(json.loads((ROOT / "private" / "launcher.json").read_text(encoding="utf-8")).get("swimsuits", True))
    except (OSError, ValueError, AttributeError):
        return True


def launcher_settings():
    """The choices launcher.sh saved, as the variables Launcher.cmd sets."""
    try:
        settings = json.loads((ROOT / "private" / "launcher.json").read_text(encoding="utf-8"))
        return {"RECOMP_D3D_SCALE": f"{int(settings.get('height', 480)) / 480:.4f}",
                "RECOMP_D3D_MSAA": str(int(settings.get("msaa", 1))),
                "RECOMP_D3D_WIDESCREEN": "1" if settings.get("widescreen") else "0",
                "RECOMP_AUDIO_GAIN": f"{int(settings.get('volume', 100)) / 100:.4f}"}
    except (OSError, ValueError, TypeError, AttributeError, OverflowError):
        return {}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--launcher", action="store_true",
                        help="Pick settings first (Linux; see launcher.sh)")
    parser.add_argument("--vsync", action="store_true",
                        help="Also synchronize presentation to the display")
    args = parser.parse_args()
    if not RUNNER.is_file():
        print(f"No runner at {RUNNER}. Run ./build_game.sh with your ISO first.", file=sys.stderr)
        return 1
    if args.launcher:
        # Linux builds the settings launcher beside the runner.
        launcher = RUNNER.parent / "recomp_launcher"
        if not launcher.is_file():
            print(f"{launcher} is missing. Rebuild with ./build_game.sh, "
                  "or start the game with ./run_game.sh.", file=sys.stderr)
            return 1
        if subprocess.run([str(launcher), str(ROOT)]).returncode != 0:
            return 1
    disc = play_disc()
    env = dict(os.environ)
    # Test-harness settings would speed the game up or stop it after a timeout.
    env.pop("RECOMP_UNPACED", None)
    env.pop("RECOMP_WATCHDOG_MS", None)
    for name, value in launcher_settings().items():
        env.setdefault(name, value)
    if swimsuits_enabled():
        for name, value in swimsuit_env().items():
            env.setdefault(name, value)
    logs = ROOT / "private" / "play-logs"
    logs.mkdir(parents=True, exist_ok=True)
    log = logs / (datetime.now().strftime("%Y%m%d-%H%M%S") + ".log")
    runner_args = [str(RUNNER), "--xbe", str(disc / "default.xbe")]
    if args.vsync:
        runner_args.append("--vsync")
    with log.open("x") as err, log.with_name(log.name + ".out").open("x") as out:
        result = subprocess.run(runner_args, cwd=ROOT, env=env, stdout=out, stderr=err)
    print(f"exit={result.returncode} log={log}")
    return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
