# SPDX-License-Identifier: GPL-3.0-or-later
"""private/keyboard.json becomes the runner's RECOMP_KEYMAP."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from package_test import build_launchers
from run_game import keyboard_map


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
