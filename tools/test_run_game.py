# SPDX-License-Identifier: GPL-3.0-or-later
"""private/keyboard.json becomes the runner's RECOMP_KEYMAP, and private/build.json
picks which installed version plays."""
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from package_test import build_launchers
from run_game import chosen_version, installed_versions, keyboard_map, main, runner


class KeyboardMapTests(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()
        self.addCleanup(self.folder.cleanup)
        self.root = Path(self.folder.name)
        (self.root / "private").mkdir()
        patcher = patch("run_game.ROOT", self.root)
        patcher.start()
        self.addCleanup(patcher.stop)

    def bind(self, text):
        (self.root / "private/keyboard.json").write_text(text, encoding="utf-8")

    def test_valid_file_binds_the_buttons_given(self):
        self.bind('{"a": "space", "start": "F5", "up": "W"}')
        self.assertEqual(keyboard_map(), "a=20,start=74,up=57")

    def test_l3_binds_like_a_button(self):
        self.bind('{"l3": "T"}')
        self.assertEqual(keyboard_map(), "l3=54")

    def test_absent_file_binds_nothing(self):
        self.assertEqual(keyboard_map(), "")

    def test_bad_json_is_refused(self):
        self.bind('{"a":')
        with self.assertRaises(ValueError):
            keyboard_map()

    def test_unknown_button_is_refused(self):
        self.bind('{"jump": "Z"}')
        with self.assertRaises(ValueError):
            keyboard_map()

    def test_unknown_key_is_refused(self):
        self.bind('{"a": "Home"}')
        with self.assertRaises(ValueError):
            keyboard_map()

    def test_f11_is_refused(self):
        self.bind('{"a": "F11"}')
        with self.assertRaises(ValueError):
            keyboard_map()

    @unittest.skipUnless(os.name == "nt", "The launchers are Windows executables")
    def test_launcher_arrows_move_rows_change_values_and_reach_the_tabs(self):
        script = r'''
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Windows.Forms
$type = [Reflection.Assembly]::LoadFile($env:RECOMP_LAUNCHER_TEST_EXE).GetType('LauncherForm')
$form = $type.GetConstructor([type[]]@([string])).Invoke(@($null))
$flags = [Reflection.BindingFlags]'NonPublic, Instance'
$type.GetField('menu', $flags).SetValue($form, 0)
$type.GetField('row', $flags).SetValue($form, 0)
$keyDown = $type.GetMethod('OnKeyDown', $flags)
function Press($key) { $keyDown.Invoke($form, @([Windows.Forms.KeyEventArgs]::new([Windows.Forms.Keys]::$key))) }
$resolution = $type.GetField('resolution', $flags).GetValue($form)
$resolution.Index = 2
Press E; Press Right; Press Down
$afterDown = $type.GetField('row', $flags).GetValue($form)
Press Up; Press Up; Press Right
"$($resolution.Index) $afterDown $($type.GetField('inPage', $flags).GetValue($form)) $($type.GetField('menu', $flags).GetValue($form))"
'''
        with tempfile.TemporaryDirectory() as folder:
            build_launchers(Path(folder))
            env = dict(os.environ, RECOMP_LAUNCHER_TEST_EXE=str(Path(folder) / "Launcher.exe"))
            result = subprocess.run(["powershell", "-NoProfile", "-NonInteractive",
                                     "-ExecutionPolicy", "Bypass", "-Command", script],
                                    env=env, capture_output=True, text=True, timeout=60)
            self.assertEqual(result.returncode, 0, result.stderr)
            # E opened VIDEO, Right stepped 1080p to 1440p, Down moved to MSAA,
            # Up twice reached the tabs, where Right turned to AUDIO.
            self.assertEqual(result.stdout.split(), ["3", "1", "False", "2"])

class InstalledVersionTests(unittest.TestCase):
    """The versions private/build.json lists, most recently built last, and the pick."""

    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()
        self.addCleanup(self.folder.cleanup)
        self.root = Path(self.folder.name)
        (self.root / "private").mkdir()
        patcher = patch("run_game.ROOT", self.root)
        patcher.start()
        self.addCleanup(patcher.stop)

    def install(self, name, play_disc, build_dir, runner_file=True, play_folder=True):
        """Record a built version and give it its runner and play folder."""
        entry = {"version": name, "play_disc": play_disc, "xbe": "default.xbe", "build_dir": build_dir}
        record_path = self.root / "private" / "build.json"
        entries = json.loads(record_path.read_text(encoding="utf-8")).get("installed", []) if record_path.is_file() else []
        record_path.write_text(json.dumps(
            {"version": name, "play_disc": play_disc, "xbe": "default.xbe",
             "installed": [e for e in entries if e.get("version") != name] + [entry]}, indent=2), encoding="utf-8")
        if runner_file:
            runner(entry).parent.mkdir(parents=True, exist_ok=True)
            runner(entry).write_text("", encoding="utf-8")
        if play_folder:
            (self.root / "private" / play_disc).mkdir(parents=True, exist_ok=True)
        return entry

    def remember(self, name):
        (self.root / "private" / "launcher.json").write_text(json.dumps({"version": name}), encoding="utf-8")

    def test_every_version_with_its_runner_and_play_folder_is_listed(self):
        self.install("USA 3.0", "play-disc", "build/recomp-program")
        self.install("Japan 3.1", "play-disc-jp", "build/recomp-program-jp")
        self.assertEqual([each["version"] for each in installed_versions()], ["USA 3.0", "Japan 3.1"])
        self.assertEqual([each["build_dir"] for each in installed_versions()],
                         ["build/recomp-program", "build/recomp-program-jp"])

    def test_a_version_without_its_runner_or_play_folder_is_not_installed(self):
        self.install("USA 3.0", "play-disc", "build/recomp-program")
        self.install("Japan 3.1", "play-disc-jp", "build/recomp-program-jp", play_folder=False)
        self.install("Europe 3.2", "play-disc-eur", "build/recomp-program-eur", runner_file=False)
        self.assertEqual([each["version"] for each in installed_versions()], ["USA 3.0"])

    def test_the_remembered_version_plays_else_the_last_built(self):
        self.install("USA 3.0", "play-disc", "build/recomp-program")
        self.install("Japan 3.1", "play-disc-jp", "build/recomp-program-jp")
        self.assertEqual(chosen_version()["version"], "Japan 3.1", "nothing remembered: the last build")
        self.remember("USA 3.0")
        self.assertEqual(chosen_version()["version"], "USA 3.0")
        self.remember("Europe 3.2")
        self.assertEqual(chosen_version()["version"], "Japan 3.1", "a forgotten version falls back to the last build")

    def test_a_named_version_plays_or_is_refused(self):
        self.install("USA 3.0", "play-disc", "build/recomp-program")
        self.install("Japan 3.1", "play-disc-jp", "build/recomp-program-jp")
        self.assertEqual(chosen_version("USA 3.0")["version"], "USA 3.0")
        self.assertIsNone(chosen_version("Europe 3.2"))

    def test_an_old_record_is_usa_in_the_single_build_folder(self):
        (self.root / "private" / "build.json").write_text(
            json.dumps({"version": "USA 3.0", "play_disc": "play-disc", "xbe": "default.xbe"}), encoding="utf-8")
        entry = {"build_dir": "build/recomp-program"}
        runner(entry).parent.mkdir(parents=True)
        runner(entry).write_text("", encoding="utf-8")
        (self.root / "private" / "play-disc").mkdir()
        self.assertEqual(installed_versions(), [{"version": "USA 3.0", "play_disc": "play-disc",
                                                 "xbe": "default.xbe", "build_dir": "build/recomp-program"}])

    def test_the_launcher_pick_plays(self):
        usa = self.install("USA 3.0", "play-disc", "build/recomp-program")
        japan = self.install("Japan 3.1", "play-disc-jp", "build/recomp-program-jp")
        (runner(japan).parent / "recomp_launcher").write_text("", encoding="utf-8")
        calls = []

        def run(command, **kwargs):
            calls.append(command)
            if len(calls) == 1:
                self.remember("USA 3.0")  # the player picks USA on the Version row
            return subprocess.CompletedProcess(command, 0)

        with patch("sys.argv", ["run_game.py", "--launcher"]), patch("run_game.subprocess.run", run), \
                patch("run_game.play_disc", lambda version: self.root / "private" / version["play_disc"]), \
                patch("builtins.print"):
            self.assertEqual(main(), 0)
        self.assertEqual(calls[1][0], str(runner(usa)))

    def test_nothing_built_plays_nothing(self):
        self.assertIsNone(chosen_version())
        self.assertIsNone(chosen_version("USA 3.0"))
