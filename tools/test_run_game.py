# SPDX-License-Identifier: GPL-3.0-or-later
"""private/keyboard.json becomes the runner's RECOMP_KEYMAP."""
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

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
