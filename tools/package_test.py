# SPDX-License-Identifier: GPL-3.0-or-later
"""Package a release ZIP (e.g. 0.1-Alpha): source, the pinned lifter and setup tools, no game data.

--platform macOS or Linux packages the same source as a .tar.gz, keeping the scripts executable."""
import argparse
import io
from pathlib import Path
import subprocess
import tarfile
import time
import zipfile

ROOT = Path(__file__).resolve().parents[1]
# Agent notes and dev-only submodule metadata stay out of the tester package.
SUBMODULES = ("tools/xboxrecomp", "third_party/smaa")
EXCLUDE = {"AGENTS.md", ".gitmodules", *SUBMODULES}
# The build reads only SMAA.hlsl and Textures; the demo is 48 MB of media.
SKIP = "third_party/smaa/Demo/"
# extract-xiso.exe and its LICENSE.TXT, copied from the DOAXBV alpha package.
ARTIFACTS = ROOT / "private" / "release-assets"


def git(*args, cwd=ROOT):
    return subprocess.check_output(["git", *args], cwd=cwd)


def archive(cwd, prefix):
    with tarfile.open(fileobj=io.BytesIO(git("archive", "--format=tar", "HEAD", cwd=cwd))) as tar:
        for member in tar.getmembers():
            if member.isfile() and member.name not in EXCLUDE:
                yield prefix + member.name, tar.extractfile(member).read(), member.mode


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("version")
    parser.add_argument("--platform", choices=("Windows", "macOS", "Linux"), default="Windows")
    args = parser.parse_args()
    version = args.version
    unix = args.platform != "Windows"
    if git("status", "--porcelain", "--untracked-files=no").strip():
        raise SystemExit("Commit first: the package is built from HEAD")
    for sub in SUBMODULES:
        pinned = git("ls-tree", "HEAD", sub).split()[2].decode()
        if git("rev-parse", "HEAD", cwd=ROOT / sub).decode().strip() != pinned or \
                git("status", "--porcelain", cwd=ROOT / sub).strip():
            raise SystemExit(f"{sub} must be a clean checkout of {pinned}")

    name = f"DOA3-{version}"
    out = ROOT / "private" / "release" / (f"{name}-{args.platform}.tar.gz" if unix else f"{name}.zip")
    if out.exists():
        raise SystemExit(f"{out} already exists")
    # macOS and Linux build extract-xiso themselves (docs/building.md).
    artifacts = () if unix else ("extract-xiso.exe", "LICENSE.TXT")
    missing = [a for a in artifacts if not (ARTIFACTS / a).is_file()]
    if missing:
        raise SystemExit(f"Missing from {ARTIFACTS}: {', '.join(missing)}")
    files = list(archive(ROOT, f"{name}/"))
    for sub in SUBMODULES:
        files += [f for f in archive(ROOT / sub, f"{name}/{sub}/") if not f[0].startswith(f"{name}/{SKIP}")]
    files += [(f"{name}/tools/artifacts/{a}", (ARTIFACTS / a).read_bytes(), 0o644) for a in artifacts]
    readme = ROOT / "docs" / ("building.md" if unix else "test-release.md")
    files.append((f"{name}/README.txt", readme.read_bytes(), 0o644))
    out.parent.mkdir(parents=True, exist_ok=True)
    partial = out.with_name(out.name + ".tmp")
    if unix:
        with tarfile.open(partial, "w:gz") as package:
            for path, data, mode in files:
                info = tarfile.TarInfo(path)
                info.size, info.mode, info.mtime = len(data), mode, int(time.time())
                package.addfile(info, io.BytesIO(data))
    else:
        with zipfile.ZipFile(partial, "w", zipfile.ZIP_DEFLATED) as package:
            for path, data, _ in files:
                package.writestr(path, data)
    partial.rename(out)
    print(out)


if __name__ == "__main__":
    main()
