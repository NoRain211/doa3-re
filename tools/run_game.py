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
