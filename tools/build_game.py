# SPDX-License-Identifier: GPL-3.0-or-later
"""Build the DOA3 runner from a user-owned ISO or extracted disc folder."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import uuid

from extract_iso import extract, run_logged, sha256

ROOT = Path(__file__).resolve().parents[1]
LIFTER = ROOT / "tools" / "xboxrecomp"
XBE_SHA256 = "e523dd15d0765899a43a1824c023e0219d129e9a8b41e8213e4e645755d2bfa3"
CAPSTONE = "5.0.7"
# The generated program the runtime was validated against. Changing the
# lifter pin, tools/doa3/*.json or program_forwards.c changes it; regenerate
# locally and update both values together.
PROGRAM_SHA256 = "73f9be4f86bc8a7ce024c38b6d907f6b129237d10aa049b246527e005ab7febc"
PROGRAM_EBP = 0


def program_manifest(generated):
    chunks = sorted(generated.glob("recomp_[0-9][0-9][0-9][0-9].c"))
    members = chunks + [generated / "recomp_dispatch.c", generated / "recomp_funcs.h"]
    manifest = "".join(f"{p.name}\t{p.stat().st_size}\t{sha256(p)}\n" for p in members)
    ebp = sum(p.read_text(encoding="utf-8").count("\n    uint32_t ebp;\n")
              for p in members if p.suffix == ".c")
    return hashlib.sha256(manifest.encode("utf-8")).hexdigest(), ebp


def find_disc(source):
    imported = ROOT / "private" / "imported-disc"
    if source is None or source.is_file():
        receipt = imported / "receipt.json"
        if source is not None and imported.exists() and (
                not receipt.is_file() or
                json.loads(receipt.read_text(encoding="utf-8")).get("iso_sha256") != sha256(source)):
            # A different ISO, or an extraction that never finished.
            if (imported / "disc" / ".recomp-storage").exists():
                raise ValueError(f"{imported} holds game saves; move them out before using another ISO")
            print(f"Replacing {imported}", flush=True)
            shutil.rmtree(imported)
        if receipt.is_file():
            print(f"Using the disc already extracted to {imported / 'disc'}", flush=True)
        elif source is None:
            raise ValueError("Drag your Dead or Alive 3 ISO or extracted disc folder onto BuildGame.cmd")
        else:
            bundled = ROOT / "tools" / "artifacts" / "extract-xiso.exe"
            extract(source, imported, bundled if bundled.is_file() else "extract-xiso")
        return imported / "disc"
    return source.resolve(strict=True)


def build(source):
    disc = find_disc(source)
    xbe = disc / "default.xbe"
    if not xbe.is_file() or sha256(xbe) != XBE_SHA256:
        raise ValueError("This build supports only Dead or Alive 3 (Xbox, USA): "
                         f"default.xbe SHA-256 {XBE_SHA256}")
    if shutil.which("cmake") is None:
        raise ValueError("Install CMake 3.20 or newer and add it to PATH")
    try:
        import capstone
        ready = capstone.__version__ == CAPSTONE
    except ImportError:
        ready = False
    if not ready:
        subprocess.check_call([sys.executable, "-m", "pip", "install", f"capstone=={CAPSTONE}"])

    work = ROOT / "private" / ("program-" + uuid.uuid4().hex[:8])
    work.mkdir(parents=True)
    disasm, generated = work / "disasm", work / "generated"
    print("Lifting default.xbe takes several minutes.", flush=True)
    analysis = work / "default_analysis.json"
    run_logged([sys.executable, "-u", "-m", "tools.xbe_parser", xbe, "--json", analysis],
               work / "xbe_parser.log", LIFTER)
    run_logged([sys.executable, "-u", "-m", "tools.disasm", xbe, "-o", disasm,
                "--analysis-json", analysis,
                "--seed-functions", ROOT / "tools/doa3/seed_functions.json"],
               work / "disasm.log", LIFTER)
    # The missing func-id/ABI/icall paths keep the lifter to the recipe's inputs.
    run_logged([sys.executable, "-u", "-m", "tools.recomp", xbe, "--all", "--split", "1000",
                "--game-name", "Dead or Alive 3",
                "--functions", disasm / "functions.json", "--labels", disasm / "labels.json",
                "--func-id-dir", work / "none", "--abi-dir", work / "none",
                "--icall-sites", work / "none.json",
                "--manual-functions", ROOT / "tools/doa3/manual_functions.json",
                "--exclude-manual", ROOT / "recomp-runtime/program_forwards.c",
                "--gen-dir", generated, "--output-dir", work / "metadata"],
               work / "lift.log", LIFTER)
    manifest, ebp = program_manifest(generated)
    if (manifest, ebp) != (PROGRAM_SHA256, PROGRAM_EBP):
        raise ValueError(f"Generated program {manifest} differs from the validated {PROGRAM_SHA256}")

    build_dir = ROOT / "build" / "recomp-program"
    run_logged(["cmake", "-S", ROOT / "recomp-runtime", "-B", build_dir, "-A", "x64",
                f"-DRECOMP_PROGRAM_DIR={generated.as_posix()}",
                f"-DRECOMP_PROGRAM_MANIFEST_SHA256={manifest}",
                f"-DRECOMP_PROGRAM_EBP_EXPECTED={ebp}"], work / "configure.log")
    run_logged(["cmake", "--build", build_dir, "--config", "Release",
                "--target", "recomp_program_runner"], work / "build.log")
    (ROOT / "private" / "disc-path.txt").write_text(str(disc) + "\n", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, nargs="?",
                        help="DOA3 ISO, or a folder holding the extracted disc's default.xbe")
    args = parser.parse_args()
    try:
        build(args.source)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f"Build failed: {error}", file=sys.stderr)
        return 1
    print("Build complete. Double-click Launcher.cmd to play.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
