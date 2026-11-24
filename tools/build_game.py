# SPDX-License-Identifier: GPL-3.0-or-later
"""Build the DOA3 runner from a user-owned ISO or extracted disc folder."""
import argparse
import gzip
import hashlib
import http.client
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time
import urllib.request
import uuid
import zipfile

from extract_iso import extract, run_logged, sha256

ROOT = Path(__file__).resolve().parents[1]
LIFTER = ROOT / "tools" / "xboxrecomp"
CAPSTONE = "5.0.7"
# Windows builds with llvm-mingw's Clang and mingw32-make, pinned the same as
# doaxbv-re and doa2u-re, and Kitware's CMake; each is fetched on first use.
WINDOWS_TOOLS = [
    ("llvm-mingw-20261006", "clang.exe",
     "https://github.com/mstorsjo/llvm-mingw/releases/download/20261006/llvm-mingw-20261006-ucrt-x86_64.zip",
     "317492c456aa27ee607a5919f1d2d38dcdc1112516a24d0bf4b00d078f52d17a"),
    ("cmake-4.4.5", "cmake.exe",
     "https://github.com/Kitware/CMake/releases/download/v4.4.5/cmake-4.4.5-windows-x86_64.zip",
     "326c826486d46a23cf9d5a1c9ee3a567d427bdd02c4a34071937a2a675cff004"),
]
# The discs the build accepts, by default.xbe SHA-256: Japan 3.1 is the main
# build, USA 3.0 is still accepted, Europe 3.2 is in bring-up. "program" is
# the generated program the runtime was validated against; changing the
# lifter pin, the recipe's JSON or its forwards file changes it, so regenerate
# locally and update it with them.
# Each version keeps its own extraction and play folder (saves), by suffix.
VERSIONS = {
    "469cb086048020d04fbc45b05fd5c470b479f06013d77d736c0ea11df5de85df": dict(
        name="Japan 3.1", recipe="doa3-jp", forwards="program_forwards_jp.c", jp=True, eur=False, suffix="-jp",
        program="310a103cd3670708576179e24d35644035e48958972859ec43af697a6ae36842", ebp=0),
    "e523dd15d0765899a43a1824c023e0219d129e9a8b41e8213e4e645755d2bfa3": dict(
        name="USA 3.0", recipe="doa3", forwards="program_forwards.c", jp=False, eur=False, suffix="",
        program="8e244632729cb546b0eea0f48368d02a7ac98412550af94e2e0d3e3ce950c519", ebp=0),
    "2d1b63b072fdefcd97174afeb28a111ce9443633f3f2a12122a8dea3fca037ab": dict(
        name="Europe 3.2", recipe="doa3-eur", forwards="program_forwards_eur.c", jp=True, eur=True,
        suffix="-eur", program="6580d45e9288f5fe260ac7f1c6144e5986721b9cef242f9f39ef2a6021e8f854", ebp=0),
}
# A movie cut to its stream and audio header sectors holds no picture, so the
# game's player ends it at once. A missing movie file stalls the game instead.
MOVIE_STUB = 0x2000
# Mod costume packs, fetched from their authors' MediaFire links at build time;
# the repository ships none, since each costume is the game's own model edited.
# Gultigargar allowed the port to bundle theirs. Name, MediaFire page, SHA-256.
COSTUME_PACKS = [
    ("Gultigargar's", "https://www.mediafire.com/file/67i91pv9b5pp1m2/",
     "99bdb540f5159d13b0fc8850a13fe634512b22925aaf749cf97eb9bc1833eecb"),
    ("usagiZ's", "https://www.mediafire.com/file/edr7w004jghne10/",
     "e5a771fb06b801b25c31ca645341d632e19b6edf37280456639af87e157428b4"),
]
# The costume files in the save folder, at most: 128 costumes, what the forum
# reports a real Xbox loads. The game keeps each one open, and the runtime's 1024
# file handles (MAX_FILE_HANDLES in recomp-runtime/kernel_file.c) fit them.
COSTUME_FILES = 256
# Booster Disc names (kasA0) and the packs' (kas17); variants like aya07-nobow are not loaded.
COSTUME_NAME = re.compile(r"[a-z]{3}(?:[0-9]{2}|A[0-9])")


def use_windows_newlines(generated):
    """PROGRAM_SHA256 authenticates the lifter's Windows output, which Python
    writes with CRLF line endings; other hosts write LF, so match those bytes."""
    if sys.platform == "win32":
        return
    for path in generated.iterdir():
        if path.is_file():
            data = path.read_bytes()
            path.write_bytes(data.replace(b"\r\n", b"\n").replace(b"\n", b"\r\n"))


def rename_retrying(source, target):
    """Antivirus scans of freshly written files can briefly lock a folder against renaming."""
    for attempt in range(31):  # 30 one-second waits
        try:
            return source.rename(target)
        except PermissionError:
            if attempt == 30:
                raise
            time.sleep(1)


def program_manifest(generated):
    chunks = sorted(generated.glob("recomp_[0-9][0-9][0-9][0-9].c"))
    members = chunks + [generated / "recomp_dispatch.c", generated / "recomp_funcs.h"]
    manifest = "".join(f"{p.name}\t{p.stat().st_size}\t{sha256(p)}\n" for p in members)
    ebp = sum(p.read_text(encoding="utf-8").count("\n    uint32_t ebp;\n")
              for p in members if p.suffix == ".c")
    return hashlib.sha256(manifest.encode("utf-8")).hexdigest(), ebp


def movies_cut(disc):
    return any(movie.stat().st_size == MOVIE_STUB for movie in disc.glob("*.sfd"))


def disc_version(disc):
    """The disc's default.xbe (any case) and its entry in VERSIONS."""
    xbe = next((p for p in disc.iterdir() if p.name.casefold() == "default.xbe"), None)
    version = VERSIONS.get(sha256(xbe)) if xbe else None
    if version is None:
        raise ValueError("This build supports Dead or Alive 3 (Xbox) Japan 3.1, USA 3.0 and Europe 3.2 only: "
                         "default.xbe is not one of them")
    return xbe, version


def extractions():
    """The build's own extractions, one per version, Japan first."""
    return [ROOT / "private" / ("imported-disc" + v["suffix"]) for v in VERSIONS.values()]


def runner_dir(version):
    """The version's own runner folder, so another version's build never replaces
    it. USA's suffix is empty, so it keeps build/recomp-program."""
    return ROOT / "build" / ("recomp-program" + version["suffix"])


def installed_versions(record):
    """A build record's installed versions, most recently built last. A record
    older than that list holds the one version its top-level keys describe,
    built into build/recomp-program."""
    installed = record.get("installed")
    if isinstance(installed, list):
        return [entry for entry in installed if isinstance(entry, dict)]
    return [{"version": record.get("version", "USA 3.0"), "play_disc": record.get("play_disc", "play-disc"),
             "xbe": record.get("xbe", "default.xbe"), "build_dir": "build/recomp-program"}]


def record_install(version, play, xbe, build_dir):
    """Add this build to private/build.json's installed list, keeping every other
    version, so the launchers can offer all of them. The top-level keys stay the
    ones the last build wrote. An entry whose runner folder this build replaced
    goes: a record older than the list kept Japan 3.1 in USA's build/recomp-program."""
    path = ROOT / "private" / "build.json"
    try:
        previous = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        previous = {}
    installed = (installed_versions(previous) if isinstance(previous, dict)
                 and (previous.get("version") or previous.get("installed")) else [])
    entry = {"version": version, "play_disc": play, "xbe": xbe,
             "build_dir": Path(build_dir).relative_to(ROOT).as_posix()}
    path.write_text(json.dumps(
        {"version": version, "play_disc": play, "xbe": xbe,
         "installed": [each for each in installed if each.get("version") != version
                       and each.get("build_dir") != entry["build_dir"]] + [entry]},
        indent=2) + "\n", encoding="utf-8")


def extracted_disc(imported):
    """An extraction's disc: its own folder until a build succeeds, then the
    play folder that hard-links every file of it."""
    disc = imported / "disc"
    return disc if disc.is_dir() else imported.with_name(imported.name.replace("imported-disc", "play-disc", 1))


def rebuild_disc():
    """Without a source: the disc the last build used, else an extraction."""
    path_file = ROOT / "private" / "disc-path.txt"
    if path_file.is_file():
        last = Path(path_file.read_text(encoding="utf-8").splitlines()[0].strip())
        if last.is_dir():
            return last
    return next((extracted_disc(i) for i in extractions()
                 if (i / "receipt.json").is_file() and extracted_disc(i).is_dir()), None)


def find_disc(source, movies=True, extractor=None):
    if source is not None and not source.is_file():
        return source.resolve(strict=True)
    if source is None:
        disc = rebuild_disc()
        if disc is None:
            raise ValueError("Choose your Dead or Alive 3 ISO or extracted disc folder to build from")
        if movies and movies_cut(disc):
            raise ValueError("This build removed the movies; build again from your ISO to restore them")
        print(f"Using the disc in {disc}", flush=True)
        return disc
    print("Checking ISO hash...", flush=True)
    iso_sha256 = sha256(source)
    for imported in extractions():
        receipt, disc = imported / "receipt.json", extracted_disc(imported)
        if (receipt.is_file() and json.loads(receipt.read_text(encoding="utf-8")).get("iso_sha256") == iso_sha256
                and disc.is_dir() and not (movies and movies_cut(disc))):
            print(f"Using the disc already extracted to {disc}", flush=True)
            return disc
    # Extract beside the others, then take the version's place, so another
    # version's extraction (and its play folder's saves) is never touched.
    staging = ROOT / "private" / "imported-disc.new"
    if staging.exists():
        shutil.rmtree(staging)
    bundled = ROOT / "tools" / "artifacts" / "extract-xiso.exe"
    try:
        extract(source, staging, extractor or (bundled if bundled.is_file() else "extract-xiso"), iso_sha256)
        imported = ROOT / "private" / ("imported-disc" + disc_version(staging / "disc")[1]["suffix"])
        # Set the old import aside until the new one is in place, so a failed
        # rename never loses both copies.
        previous = imported.with_name(imported.name + ".old")
        if previous.exists():
            if imported.exists():
                shutil.rmtree(previous)
            else:  # a run that died between the renames left the only copy aside
                rename_retrying(previous, imported)
        if imported.exists():
            # The same version from another ISO, an unfinished extraction, or one whose movies were cut.
            if (imported / "disc" / ".recomp-storage").exists():
                raise ValueError(f"{imported} holds game saves; move them out before using another ISO")
            print(f"Replacing {imported}", flush=True)
            rename_retrying(imported, previous)
        try:
            rename_retrying(staging, imported)
        except BaseException:
            if previous.exists():
                rename_retrying(previous, imported)
            raise
        shutil.rmtree(previous, ignore_errors=True)
    finally:
        # A failed extraction or unsupported disc must not leave staging behind.
        # The next run removes a leftover, so a locked folder must not hide the real error.
        shutil.rmtree(staging, ignore_errors=True)
    return imported / "disc"


def prepare_play_disc(disc, movies, name="play-disc"):
    """Hard-link the disc into private/<name>, which keeps the game's cache and
    saves (.recomp-storage) out of the source. Without movies each .sfd becomes a
    stub copied from the source, which is left whole."""
    play = ROOT / "private" / name
    # Fill a staging folder so an interrupted first fill is never taken as complete.
    folder = play if play.is_dir() else play.with_name(name + ".tmp")
    for file in sorted(disc.rglob("*")):
        relative = file.relative_to(disc)
        if not file.is_file() or relative.parts[0].casefold() == ".recomp-storage":
            continue
        size = file.stat().st_size
        cut = not movies and file.suffix.casefold() == ".sfd" and size > MOVIE_STUB
        target = folder / relative
        if target.is_file() and target.stat().st_size == (MOVIE_STUB if cut else size):
            continue
        target.parent.mkdir(parents=True, exist_ok=True)
        partial = target.with_name(target.name + ".tmp")
        partial.unlink(missing_ok=True)
        if cut:
            with file.open("rb") as movie:
                partial.write_bytes(movie.read(MOVIE_STUB))
        else:
            try:
                os.link(file, partial)
            except OSError:  # another drive
                shutil.copyfile(file, partial)
        os.replace(partial, target)
    if folder != play:
        rename_retrying(folder, play)


def drop_owned_extraction(disc, play):
    """The play folder now holds every file of the extraction this build made,
    as hard links (which Explorer counts twice) or movie stubs. Drop the
    extraction's disc, keep its receipt, and use the play folder as the disc
    from now on. A user's own folder, or one holding saves, is never changed."""
    if (disc.resolve() not in [(i / "disc").resolve() for i in extractions()]
            or (disc / ".recomp-storage").exists()):
        return disc
    # Renamed first, so a file a scan holds open never leaves a partial disc behind.
    aside = disc.with_name("disc.old")
    try:
        shutil.rmtree(aside, ignore_errors=True)
        rename_retrying(disc, aside)  # also fails on a disc.old the rmtree could not clear
    except OSError as error:  # the play folder is complete; dropping only frees space
        print(f"Kept {disc}: {error}", flush=True)
        return disc
    shutil.rmtree(aside, ignore_errors=True)
    return play


def ask_movies(source):
    last = rebuild_disc() if source is None else None
    if last is not None and movies_cut(last):
        print("Rebuilding without movies; build again from your ISO to restore them.", flush=True)
        return False
    if not sys.stdin.isatty():
        return True
    try:
        answer = input("Install the movies? They take 3.4 GB; without them the game skips "
                       "every movie. [Y/n] ")
    except EOFError:
        return True
    return not answer.strip().casefold().startswith("n")


def physical_memory():
    """Installed memory in bytes, or 0 when it cannot be read."""
    try:
        if sys.platform == "win32":
            import ctypes
            class MemoryStatus(ctypes.Structure):
                _fields_ = [("length", ctypes.c_uint32), ("load", ctypes.c_uint32)] + [
                    (name, ctypes.c_uint64) for name in ("total", "available", "page_file", "page_file_available",
                                                         "virtual", "virtual_available", "extended")]
            status = MemoryStatus(length=ctypes.sizeof(MemoryStatus))
            return status.total if ctypes.windll.kernel32.GlobalMemoryStatusEx(ctypes.byref(status)) else 0
        return os.sysconf("SC_PAGE_SIZE") * os.sysconf("SC_PHYS_PAGES")
    except (AttributeError, OSError, ValueError):
        return 0


def build_jobs():
    """Compile jobs: a generated chunk takes up to about 1 GB to compile, so one
    job per 4 GB of memory, and at least the 4 a 16 GB Steam Deck runs."""
    return min(os.cpu_count() or 1, max(4, physical_memory() >> 32))


def windows_tools():
    """The pinned Windows tools' bin folders, each downloaded into private/ on first use."""
    bins = []
    for name, probe, url, expected in WINDOWS_TOOLS:
        home = ROOT / "private" / name
        bins.append(home / "bin")
        if (home / "bin" / probe).exists():
            continue
        print(f"Downloading {name} (first build only).", flush=True)
        # Unpacked beside its final name and renamed last, so a broken download never looks complete.
        staging = home.with_name(home.name + ".tmp")
        shutil.rmtree(staging, ignore_errors=True)
        staging.mkdir(parents=True)
        archive = staging / "download.zip"
        urllib.request.urlretrieve(url, archive)
        if sha256(archive) != expected:
            shutil.rmtree(staging)
            raise ValueError(f"The downloaded {url.rsplit('/', 1)[1]} does not match its pinned SHA-256")
        with zipfile.ZipFile(archive) as contents:
            contents.extractall(staging / "unpacked")
        (top,) = (staging / "unpacked").iterdir()  # each ZIP holds one versioned folder
        shutil.rmtree(home, ignore_errors=True)
        rename_retrying(top, home)
        shutil.rmtree(staging)
    return bins


def pick_costumes(offered, present):
    """The (name, .xpr path) costumes to install from those offered, in pack
    order, beside the costume files already present. Characters take turns, so
    each gets a share of what fits; a present name is never replaced."""
    queues = {}
    for name, path in offered:
        if name not in present and name not in queues.get(name[:3], {}):
            queues.setdefault(name[:3], {})[name] = path
    room = (COSTUME_FILES - len(present) * 2) // 2
    picked = []
    while room > len(picked) and any(queues.values()):
        for queue in queues.values():
            if queue and room > len(picked):
                name = next(iter(queue))
                picked.append((name, queue.pop(name)))
    return picked


def unpack(archive, folder):
    """Unpack a ZIP, or a RAR with bsdtar: Windows' and macOS's tar."""
    if zipfile.is_zipfile(archive):
        with zipfile.ZipFile(archive) as contents:
            contents.extractall(folder)
        return
    tar = shutil.which("bsdtar") or (
        str(Path(os.environ.get("SystemRoot", r"C:\Windows")) / "System32" / "tar.exe")
        if sys.platform == "win32" else None)
    if tar is None:
        raise ValueError("unpacking a RAR needs bsdtar; install libarchive-tools")
    subprocess.run([tar, "-xf", archive, "-C", folder], check=True)


def download_mediafire(page, path):
    """MediaFire's download link expires, so it is read from the file's page each time."""
    def fetch(url):
        return urllib.request.urlopen(urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"}),
                                      timeout=60)
    with fetch(page) as response:
        link = re.search(r'href="(https://download[^"]+)"', response.read().decode("utf-8", "replace"))
    if link is None:
        raise ValueError(f"no download link on {page}")
    with fetch(link.group(1)) as response, path.open("wb") as file:
        shutil.copyfileobj(response, file)


def install_costumes(play):
    """Add the mod costume packs to the play folder's title save folder, once:
    a costume the player deletes stays deleted. A pack that fails is skipped,
    and the next build tries it again."""
    storage = ROOT / "private" / play / ".recomp-storage"
    saves = storage / "partition1" / "TDATA" / "54430001"
    record = storage / "costume-packs.txt"
    done = set(record.read_text(encoding="utf-8").splitlines()) if record.is_file() else set()
    staging = ROOT / "private" / "costume-packs.tmp"
    shutil.rmtree(staging, ignore_errors=True)
    offered, fetched = [], []
    try:
        for index, (name, page, expected) in enumerate(COSTUME_PACKS):
            if page in done:
                continue
            print(f"Downloading {name} mod costumes.", flush=True)
            folder = staging / str(index)
            folder.mkdir(parents=True)
            archive = staging / f"{index}.download"
            try:
                download_mediafire(page, archive)
                if sha256(archive) != expected:
                    raise ValueError("the download does not match its pinned SHA-256")
                unpack(archive, folder)
            except (OSError, ValueError, subprocess.CalledProcessError, http.client.HTTPException) as error:
                print(f"Skipped {name} mod costumes: {error}", flush=True)
                continue
            fetched.append(page)
            offered += sorted((xpr.stem, xpr) for xpr in folder.rglob("*.xpr")
                              if COSTUME_NAME.fullmatch(xpr.stem) and xpr.with_suffix(".cat").is_file())
        if not fetched:
            return
        saves.mkdir(parents=True, exist_ok=True)
        present = {file.stem for file in saves.iterdir() if file.suffix in (".xpr", ".cat")}
        picked = pick_costumes(offered, present)
        for stem, xpr in picked:
            for source in (xpr.with_suffix(".cat"), xpr):
                partial = saves / (source.name + ".tmp")
                shutil.copyfile(source, partial)
                os.replace(partial, saves / source.name)
        record.write_text("".join(f"{page}\n" for page in sorted(done | set(fetched))), encoding="utf-8")
        print(f"Installed {len(picked)} mod costumes of {len(offered)}; the save folder holds "
              f"{COSTUME_FILES // 2} costumes at most.", flush=True)
    finally:
        shutil.rmtree(staging, ignore_errors=True)


def build(source, movies=True, extractor=None, costumes=True):
    windows = sys.platform == "win32"
    disc = find_disc(source, movies, extractor)
    xbe, version = disc_version(disc)
    play = "play-disc" + version["suffix"]
    print(f"Dead or Alive 3 {version['name']}", flush=True)
    print(f"Linking the disc into private/{play}" + ("" if movies else " without movies"), flush=True)
    prepare_play_disc(disc, movies, play)
    if windows:
        os.environ["PATH"] = os.pathsep.join(map(str, windows_tools() + [os.environ["PATH"]]))
        shutil.rmtree(ROOT / "private" / "w64devkit-2.11.0", ignore_errors=True)  # 0.5's toolchain, 720 MB
    elif shutil.which("cmake") is None:
        raise ValueError("Install CMake 3.20 or newer and add it to PATH")
    elif shutil.which("ninja") is None:
        raise ValueError("Install Ninja and add it to PATH")
    try:
        import capstone
        ready = capstone.__version__ == CAPSTONE
    except ImportError:
        ready = False
    if not ready:
        subprocess.check_call([sys.executable, "-m", "pip", "install", f"capstone=={CAPSTONE}"])

    work = ROOT / "private" / ("program-" + uuid.uuid4().hex[:8])
    work.mkdir(parents=True)
    generated = work / "generated"
    recipe = ROOT / "tools" / version["recipe"]
    # The recipe's stored disassembly result spares every build about three minutes.
    functions = work / "functions.json"
    functions.write_bytes(gzip.decompress((recipe / "functions.json.gz").read_bytes()))
    print(f"Lifting {xbe.name} takes a few minutes.", flush=True)
    # The missing disasm/func-id/ABI/icall paths keep the lifter to the recipe's inputs.
    run_logged([sys.executable, "-u", "-m", "tools.recomp", xbe, "--all", "--split", "1000",
                "--game-name", "Dead or Alive 3",
                "--functions", functions, "--disasm-dir", work / "none",
                "--func-id-dir", work / "none", "--abi-dir", work / "none",
                "--icall-sites", work / "none.json",
                "--manual-functions", recipe / "manual_functions.json",
                "--exclude-manual", ROOT / "recomp-runtime" / version["forwards"],
                "--gen-dir", generated, "--output-dir", work / "metadata"],
               work / "lift.log", LIFTER)
    use_windows_newlines(generated)
    manifest, ebp = program_manifest(generated)
    if (manifest, ebp) != (version["program"], version["ebp"]):
        raise ValueError(f"Generated program {manifest} differs from the validated {version['program']}")

    build_dir = runner_dir(version)
    # A build folder cannot change generator; earlier Windows builds used
    # Visual Studio, then Ninja with w64devkit's GCC.
    generator = ["-G", "MinGW Makefiles" if windows else "Ninja", "-DCMAKE_BUILD_TYPE=Release"]
    cache = build_dir / "CMakeCache.txt"
    if cache.exists() and f"CMAKE_GENERATOR:INTERNAL={generator[1]}\n" not in cache.read_text(errors="replace"):
        shutil.rmtree(build_dir)
    if windows:
        generator += ["-DCMAKE_C_COMPILER=clang", "-DCMAKE_CXX_COMPILER=clang++"]
    if sys.platform.startswith("linux"):
        generator.append("-DRECOMP_FETCH_SDL3=ON")  # used only when the system has no SDL3
    run_logged(["cmake", "-S", ROOT / "recomp-runtime", "-B", build_dir, *generator,
                f"-DRECOMP_PROGRAM_DIR={generated.as_posix()}",
                f"-DRECOMP_PROGRAM_MANIFEST_SHA256={manifest}",
                f"-DRECOMP_PROGRAM_EBP_EXPECTED={ebp}",
                f"-DRECOMP_DOA3_JP={'ON' if version['jp'] else 'OFF'}",
                f"-DRECOMP_DOA3_EUR={'ON' if version['eur'] else 'OFF'}"], work / "configure.log")
    # Linux builds the settings launcher beside the runner; SteamOS has no compiler.
    # Windows also builds the Vulkan runner, which the launcher's Renderer setting picks.
    targets = ["recomp_program_runner"] + (["recomp_launcher"] if sys.platform.startswith("linux") else [])
    if windows:
        targets.append("recomp_program_runner_vulkan")
    run_logged(["cmake", "--build", build_dir, "--parallel", str(build_jobs()),
                "--target", *targets], work / "build.log")
    disc = drop_owned_extraction(disc, ROOT / "private" / play)
    (ROOT / "private" / "disc-path.txt").write_text(str(disc) + "\n", encoding="utf-8")
    # What is installed, for the launchers and the play scripts.
    record_install(version["name"], play, xbe.name, build_dir)
    # Earlier builds' generated programs; the build folder now uses this one.
    for old in (ROOT / "private").glob("program-????????"):
        if old != work:
            shutil.rmtree(old, ignore_errors=True)
    if costumes and not version["eur"]:  # Europe saves under another title ID
        try:
            install_costumes(play)
        except OSError as error:  # the game is built; the costumes are extras
            print(f"Mod costumes not installed: {error}", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, nargs="?",
                        help="DOA3 ISO, or a folder holding the extracted disc's default.xbe")
    parser.add_argument("--movies", action=argparse.BooleanOptionalAction,
                        help="Install the movies (3.4 GB); asks when omitted")
    parser.add_argument("--extractor", help="extract-xiso to use instead of the one on PATH")
    parser.add_argument("--costumes", action=argparse.BooleanOptionalAction, default=True,
                        help="Add Gultigargar's and usagiZ's mod costumes (about 220 MB download) once")
    args = parser.parse_args()
    try:
        build(args.source, ask_movies(args.source) if args.movies is None else args.movies,
              args.extractor, args.costumes)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f"Build failed: {error}", file=sys.stderr)
        return 1
    installed = json.loads((ROOT / "private" / "build.json").read_text(encoding="utf-8"))["version"]
    print(f"Build complete: Dead or Alive 3 {installed}. "
          + ("Choose START GAME in Launcher.exe, or run DOA3.exe, to play." if sys.platform == "win32"
             else "Run ./launcher.sh to play."))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
