# SPDX-License-Identifier: GPL-3.0-or-later
"""Package a release ZIP (e.g. 0.1-Alpha): source, the pinned lifter and setup tools, no game data."""
import argparse
import io
from pathlib import Path
import subprocess
import tarfile
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
                yield prefix + member.name, tar.extractfile(member).read()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("version")
    version = parser.parse_args().version
    if git("status", "--porcelain", "--untracked-files=no").strip():
        raise SystemExit("Commit first: the package is built from HEAD")
    for sub in SUBMODULES:
        pinned = git("ls-tree", "HEAD", sub).split()[2].decode()
        if git("rev-parse", "HEAD", cwd=ROOT / sub).decode().strip() != pinned or \
                git("status", "--porcelain", cwd=ROOT / sub).strip():
            raise SystemExit(f"{sub} must be a clean checkout of {pinned}")

    name = f"DOA3-{version}"
    out = ROOT / "private" / "release" / f"{name}.zip"
    out.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(out, "x", zipfile.ZIP_DEFLATED) as package:
        for path, data in archive(ROOT, f"{name}/"):
            package.writestr(path, data)
        for sub in SUBMODULES:
            for path, data in archive(ROOT / sub, f"{name}/{sub}/"):
                if not path.startswith(f"{name}/{SKIP}"):
                    package.writestr(path, data)
        for artifact in ("extract-xiso.exe", "LICENSE.TXT"):
            package.write(ARTIFACTS / artifact, f"{name}/tools/artifacts/{artifact}")
        package.write(ROOT / "docs" / "test-release.md", f"{name}/README.txt")
    print(out)


if __name__ == "__main__":
    main()
