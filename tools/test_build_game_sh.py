# SPDX-License-Identifier: GPL-3.0-or-later
"""build_game.sh's Linux check for the build tools before it builds."""
from pathlib import Path
import platform
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
STUB = "#!/bin/sh\nexit 0\n"
# Records the arguments build_game.sh hands tools/build_game.py; answers every other call.
PYTHON = '#!/bin/sh\ncase "$1" in */build_game.py) shift; echo "$@" > "$RAN";; esac\nexit 0\n'


@unittest.skipUnless(platform.system() == "Linux", "the tool check runs on Linux")
class BuildGameShTest(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)
        shutil.copy(ROOT / "build_game.sh", self.root)
        self.bin = self.root / "bin"
        self.bin.mkdir()
        # Only the commands the script itself needs, so no host build tools are visible.
        for tool in ("dirname", "uname", "grep", "rm"):
            (self.bin / tool).symlink_to(shutil.which(tool))

    def run_build(self, *tools, args=("/x.iso",)):
        for tool in ("git", "cmake", "ninja", "c++", "g++", "clang++", "python3", "extract-xiso"):
            (self.bin / tool).unlink(missing_ok=True)
        ran = self.root / "ran"
        ran.unlink(missing_ok=True)
        for tool in tools:
            (self.bin / tool).write_text(PYTHON if tool == "python3" else STUB)
            (self.bin / tool).chmod(0o755)
        result = subprocess.run([shutil.which("bash"), str(self.root / "build_game.sh"), *args],
                                env={"PATH": str(self.bin), "RAN": str(ran)}, stdin=subprocess.DEVNULL,
                                capture_output=True, text=True)
        return result, ran.read_text().strip() if ran.exists() else None

    def test_without_a_terminal_names_the_missing_tools_and_stops(self):
        result, ran = self.run_build()
        self.assertEqual((result.returncode, ran), (1, None))
        self.assertIn("Missing build tools: git cmake ninja a C++ compiler Python's venv.", result.stderr)
        self.assertIn("Run ./install_deps.sh in a terminal", result.stderr)

    def test_any_one_compiler_is_enough(self):
        for compiler in ("c++", "g++", "clang++"):
            with self.subTest(compiler=compiler):
                result, ran = self.run_build("git", "cmake", "ninja", "python3", "extract-xiso", compiler)
                self.assertEqual((result.returncode, ran), (0, "/x.iso"), result.stderr)
        result, ran = self.run_build("git", "cmake", "ninja", "python3", "extract-xiso")
        self.assertIn("Missing build tools: a C++ compiler.", result.stderr)

    def test_bsdtar_is_installed_from_a_terminal_and_never_stops_a_build(self):
        tools = ("git", "cmake", "ninja", "python3", "c++", "extract-xiso")
        result, ran = self.run_build(*tools)
        self.assertEqual((result.returncode, ran), (0, "/x.iso"), result.stderr)
        # script gives the build a terminal; install_deps.sh is a stub that records it ran, then fails or not.
        script = shutil.which("script")
        if script is None:
            self.skipTest("the terminal case needs util-linux's script")
        installed = self.root / "installed"
        command = f"'{shutil.which('bash')}' '{self.root / 'build_game.sh'}' /x.iso"
        for status in (0, 1):
            with self.subTest(install_status=status):
                installed.unlink(missing_ok=True)
                (self.root / "ran").unlink(missing_ok=True)
                (self.root / "install_deps.sh").write_text(f"#!/bin/sh\n: > '{installed}'\nexit {status}\n")
                (self.root / "install_deps.sh").chmod(0o755)
                result = subprocess.run([script, "-qec", command, "/dev/null"],
                                        env={"PATH": str(self.bin), "RAN": str(self.root / "ran")},
                                        stdin=subprocess.DEVNULL, capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout)
                self.assertTrue(installed.exists())
                self.assertTrue((self.root / "ran").exists(), result.stdout)
                self.assertEqual((self.root / "ran").read_text().strip(), "/x.iso")
                self.assertEqual("Could not install bsdtar" in result.stdout, status == 1, result.stdout)

    def test_builds_extract_xiso_only_for_an_iso(self):
        tools = ("git", "cmake", "ninja", "python3", "c++")
        iso = self.root / "doa3.iso"
        iso.touch()
        for args, extractor in (((str(self.root),), False), ((), False), ((str(iso),), True)):
            with self.subTest(args=args):
                result, ran = self.run_build(*tools, args=args)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual("--extractor" in ran, extractor, ran)


if __name__ == "__main__":
    unittest.main()
