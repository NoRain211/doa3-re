# SPDX-License-Identifier: GPL-3.0-or-later
"""Package a release ZIP (e.g. 0.1-Alpha): source, the pinned lifter and setup tools, no game data.

--platform macOS or Linux packages the same source as a .tar.gz, keeping the scripts executable."""
import argparse
import hashlib
import io
import os
from pathlib import Path
import re
import subprocess
import tarfile
import tempfile
import time
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]
# Agent notes and dev-only submodule metadata stay out of the tester package.
SUBMODULES = ("tools/xboxrecomp", "third_party/smaa")
EXCLUDE = {"AGENTS.md", ".gitmodules", *SUBMODULES}
# The build reads only SMAA.hlsl and Textures; the demo is 48 MB of media.
SKIP = "third_party/smaa/Demo/"
# extract-xiso.exe and its LICENSE.TXT, copied from the DOAXBV alpha package.
ARTIFACTS = ROOT / "private" / "release-assets"
# The C# compiler every Windows ships builds both exes from tools/launcher.cs and launcher_update.cs.
CSC = Path(os.environ.get("WINDIR", r"C:\Windows")) / "Microsoft.NET" / "Framework64" / "v4.0.30319" / "csc.exe"
LAUNCHERS = {"Launcher.exe": [], "DOA3.exe": ["/define:PLAY_ONLY"]}
# Optional private icon: fan art with Tecmo's characters and logo, so it stays untracked. The exes embed the
# .ico; the macOS and Linux launchers load the .png from the package's private/ folder.
ICON = ROOT / "private" / "launcher-icon.ico"
ICON_PNG = ROOT / "private" / "launcher-icon.png"
# The Windows ZIP carries Python with Capstone, so players install nothing. Each is (URL, SHA-256,
# the archive folder that ships); the wheel is installed by unpacking it. Capstone matches build_game.CAPSTONE.
PYTHON = ("https://www.nuget.org/api/v2/package/python/3.12.10",
          "0eb85c2dfccccf1b17352de4c397f69194035b7d37149eacc16f1147d93de3b8", "tools/")
WHEELS = [("https://files.pythonhosted.org/packages/70/39/2138d890a8e827636b9de9924fbc8527fe83e38b6d26605b30ac55e30ebe/"
           "capstone-5.0.7-py3-none-win_amd64.whl",
           "4ab8bcb7da8f221ff45926ca168ca33e76f7237d06fbf3c10780002faa2670e1", "")]


def git(*args, cwd=ROOT):
    return subprocess.check_output(["git", *args], cwd=cwd)


def bundle(prefix, url, digest, top):
    """(path, data, mode) for a pinned upstream ZIP's folder top under prefix, downloaded once into ARTIFACTS."""
    cached = ARTIFACTS / "downloads" / digest
    if not cached.is_file() or hashlib.sha256(cached.read_bytes()).hexdigest() != digest:
        cached.parent.mkdir(parents=True, exist_ok=True)
        partial = cached.with_name(digest + ".tmp")
        urllib.request.urlretrieve(url, partial)
        if hashlib.sha256(partial.read_bytes()).hexdigest() != digest:
            partial.unlink()
            raise SystemExit(f"{url} does not match its pinned SHA-256")
        partial.replace(cached)
    with zipfile.ZipFile(cached) as source:
        return [(prefix + info.filename[len(top):], source.read(info), 0o644)
                for info in source.infolist() if info.filename.startswith(top) and not info.is_dir()]


def archive(cwd, prefix):
    with tarfile.open(fileobj=io.BytesIO(git("archive", "--format=tar", "HEAD", cwd=cwd))) as tar:
        for member in tar.getmembers():
            if member.isfile() and member.name not in EXCLUDE:
                yield prefix + member.name, tar.extractfile(member).read(), member.mode


def build_launchers(out_dir, version=None):
    """Launcher.exe (build, settings, play) and DOA3.exe (play) into out_dir."""
    if not CSC.is_file():
        raise SystemExit(f"The launchers need Windows: {CSC} is missing")
    out_dir.mkdir(parents=True, exist_ok=True)
    sources = [str(ROOT / "tools" / name) for name in ("launcher.cs", "launcher_update.cs")]
    with tempfile.TemporaryDirectory() as stamp_dir:
        if version:
            if not re.fullmatch(r"\d+(\.\d+){1,3}(-[0-9A-Za-z.]+)?", version):
                raise SystemExit(f"{version} is not a release version such as 0.6.3")
            # The release version in the exes' file details; 0.6.2 becomes file version 0.6.2.0.
            stamp = Path(stamp_dir) / "version.cs"
            stamp.write_text(f'[assembly: System.Reflection.AssemblyFileVersion("{version.split("-")[0]}")]\n'
                             f'[assembly: System.Reflection.AssemblyInformationalVersion("{version}")]\n')
            sources.append(str(stamp))
        icon = [f"/win32icon:{ICON}"] if ICON.is_file() else []
        for exe, flags in LAUNCHERS.items():
            subprocess.run([str(CSC), "/nologo", "/warnaserror", "/target:winexe", "/r:System.Web.Extensions.dll",
                            *icon, *flags, f"/out:{out_dir / exe}", *sources], check=True)


def check_signed(folder, version):
    """Refuses launchers in folder that are not validly signed for this version."""
    for exe in LAUNCHERS:
        path = folder / exe
        if not path.is_file():
            raise SystemExit(f"{path} is missing")
        # Windows PowerShell cannot load its own modules from PowerShell 7's PSModulePath.
        env = {k: v for k, v in os.environ.items() if k.upper() != "PSMODULEPATH"}
        found = subprocess.run(
            ["powershell", "-NoProfile", "-Command",
             "(Get-AuthenticodeSignature -LiteralPath $env:SIGNED_EXE).Status; "
             "(Get-Item -LiteralPath $env:SIGNED_EXE).VersionInfo.ProductVersion"],
            env={**env, "SIGNED_EXE": str(path)}, capture_output=True, text=True, check=True).stdout.split()
        if found != ["Valid", version]:
            raise SystemExit(f"{path}: expected a valid signature on version {version}, found {' '.join(found)}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("version", nargs="?")
    parser.add_argument("--platform", choices=("Windows", "macOS", "Linux"), default="Windows")
    parser.add_argument("--launchers", type=Path, metavar="DIR",
                        help="only build Launcher.exe and DOA3.exe into DIR (the repository root for a checkout); "
                             "a version goes into their file details")
    parser.add_argument("--signed", type=Path, metavar="DIR",
                        help="package the signed Launcher.exe and DOA3.exe from DIR "
                             "(the sign-launchers workflow's artifact) instead of building them")
    args = parser.parse_args()
    if args.launchers:
        build_launchers(args.launchers, args.version)
        return
    if not args.version:
        parser.error("give a version, or --launchers DIR")
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
    # macOS and Linux build extract-xiso themselves (docs/building.md). The Linux launcher is prebuilt by
    # tools/build_linux_launcher.sh so its GAME page builds on a machine with no compiler.
    artifacts = ("launcher-linux-x86_64",) if args.platform == "Linux" else () if unix else ("extract-xiso.exe", "LICENSE.TXT")
    missing = [a for a in artifacts if not (ARTIFACTS / a).is_file()]
    # A signed build embedded the .ico on CI; every other release, and every
    # macOS or Linux package (which ships the PNG), needs it here.
    if unix or not args.signed:
        missing += [str(i.relative_to(ROOT)) for i in (ICON_PNG if unix else ICON,) if not i.is_file()]
    if missing:
        raise SystemExit(f"Missing from {ARTIFACTS}: {', '.join(missing)}")
    files = list(archive(ROOT, f"{name}/"))
    for sub in SUBMODULES:
        files += [f for f in archive(ROOT / sub, f"{name}/{sub}/") if not f[0].startswith(f"{name}/{SKIP}")]
    files += [(f"{name}/tools/artifacts/{a}", (ARTIFACTS / a).read_bytes(), 0o755 if a.startswith("launcher-") else 0o644)
              for a in artifacts]
    if unix:
        files.append((f"{name}/private/launcher-icon.png", ICON_PNG.read_bytes(), 0o644))
    if not unix:
        files += bundle(f"{name}/tools/python/", *PYTHON)
        for wheel in WHEELS:
            files += bundle(f"{name}/tools/python/Lib/site-packages/", *wheel)
        with tempfile.TemporaryDirectory() as built:
            launchers = args.signed or Path(built)
            if args.signed:
                check_signed(launchers, version)
            else:
                build_launchers(launchers, version)
            files += [(f"{name}/{exe}", (launchers / exe).read_bytes(), 0o755) for exe in LAUNCHERS]
    readme = ROOT / "docs" / ("building.md" if unix else "test-release.md")
    files.append((f"{name}/README.txt", readme.read_bytes(), 0o644))
    # tools/update.py reads the installed version from here; a checkout has no VERSION and is never updated.
    files.append((f"{name}/VERSION", f"{version}\n".encode(), 0o644))
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
