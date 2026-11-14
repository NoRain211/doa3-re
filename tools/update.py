# SPDX-License-Identifier: GPL-3.0-or-later
"""Check GitHub for a newer release package of this platform and unpack it over the install.

The launchers run this: "check" prints the newer version, if any; "install" unpacks it.
Everything it prints goes to stdout, and it exits 1 on failure with the reason as the last line.
The package's VERSION file says what is installed; a source checkout has none and is never updated.
Windows releases are a .zip; macOS and Linux releases are a .tar.gz (tools/package_test.py).
"""
import hashlib
import json
from pathlib import Path
import re
import sys
import tarfile
import tempfile
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]
LATEST = "https://api.github.com/repos/NoRain211/doa3-re/releases/latest"
SUFFIX = {"win32": ".zip", "darwin": "-macOS.tar.gz"}.get(sys.platform, "-Linux.tar.gz")


def number(text):
    found = re.search(r"\d+(\.\d+)+", text)
    return tuple(int(part) for part in found.group().split(".")) if found else (0,)


def installed():
    version = ROOT / "VERSION"
    return version.read_text(encoding="utf-8").strip() if version.is_file() else None


def fetch(url):
    return urllib.request.urlopen(urllib.request.Request(url, headers={"User-Agent": "DOA3-launcher"}), timeout=30)


def pick_asset(assets):
    """This platform's package among a release's assets."""
    for asset in assets:
        if asset["name"].endswith(SUFFIX):
            return asset
    raise ValueError("The release has no package for this platform")


def newer(current):
    """The release (tag_name, assets) when it is newer than the installed version, else None."""
    with fetch(LATEST) as response:
        release = json.load(response)
    return release if number(release["tag_name"]) > number(current) else None


def members(package):
    """(name, mode, read) for each file of an open ZIP or .tar.gz; read() returns its bytes."""
    if isinstance(package, zipfile.ZipFile):
        for entry in package.infolist():
            if not entry.is_dir():
                yield entry.filename, (entry.external_attr >> 16) & 0o777, lambda entry=entry: package.read(entry)
    else:
        for entry in package:
            if entry.isfile():
                yield entry.name, entry.mode & 0o777, package.extractfile(entry).read


def install(asset):
    digest = hashlib.sha256()
    with tempfile.TemporaryFile() as download:
        with fetch(asset["browser_download_url"]) as response:
            while chunk := response.read(1 << 20):
                download.write(chunk)
                digest.update(chunk)
        # GitHub lists the checksum as "sha256:<hex>".
        if asset.get("digest") and asset["digest"] != "sha256:" + digest.hexdigest():
            raise ValueError("The download does not match GitHub's checksum")
        download.seek(0)
        if asset["name"].endswith(".zip"):
            with zipfile.ZipFile(download) as package:
                unpack(package)
        else:
            with tarfile.open(fileobj=download, mode="r:gz") as package:
                unpack(package)


def unpack(package):
    """Writes the package over the install. Its entries sit in one top folder, DOA3-<version>/.

    Every entry is checked and read before the first write, and VERSION goes last, so an install that
    fails part way still reports the old version and can be run again."""
    files = []
    for name, mode, read in members(package):
        target = (ROOT / name.partition("/")[2]).resolve()
        if ROOT not in target.parents:
            raise ValueError(f"Unsafe path in the update: {name}")
        # private/ holds the player's files; a release only adds ones they lack.
        if ROOT / "private" in target.parents and target.exists():
            continue
        data = read()
        # Unchanged files stay untouched. Windows has no mode bits beyond read-only, so only other systems compare modes.
        if (target.is_file() and target.read_bytes() == data
                and (not mode or sys.platform == "win32" or (target.stat().st_mode & 0o777) == mode)):
            continue
        files.append((target, data, mode))
    files.sort(key=lambda file: file[0] == ROOT / "VERSION")
    for target, data, mode in files:
        target.parent.mkdir(parents=True, exist_ok=True)
        # A running exe can be renamed but not overwritten; main() removes the old one next time.
        old = target.with_name(target.name + ".old")
        swap = target.suffix == ".exe" and target.parent == ROOT and target.exists()
        if swap:
            target.replace(old)
        try:
            target.write_bytes(data)
        except OSError:
            if swap:
                old.replace(target)
            raise
        if mode:
            target.chmod(mode)


def main(argv):
    if len(argv) != 1 or argv[0] not in ("check", "install"):
        sys.exit("usage: update.py check|install")
    for old in ROOT.glob("*.exe.old"):
        try:
            old.unlink()
        except OSError:
            pass
    try:
        current = installed()
        if current is None:
            print("Updates are for the release package; this is a source checkout")
            return 1
        release = newer(current)
        if release is not None:
            asset = pick_asset(release["assets"])
            if argv[0] == "install":
                install(asset)
            print(release["tag_name"].lstrip("v"))
    except (OSError, ValueError, KeyError, zipfile.BadZipFile, tarfile.TarError) as error:
        print(f"Update failed: {error}")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
