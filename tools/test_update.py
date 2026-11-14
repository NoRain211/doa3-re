# SPDX-License-Identifier: GPL-3.0-or-later
import io
from pathlib import Path
import tarfile
import tempfile
import unittest
from unittest import mock
import zipfile

import update


def archive(files):
    data = io.BytesIO()
    with zipfile.ZipFile(data, "w") as zipped:
        for name, content in files.items():
            zipped.writestr(name, content)
    return zipfile.ZipFile(data)


def tarball(files, modes=None):
    data = io.BytesIO()
    with tarfile.open(fileobj=data, mode="w:gz") as tarred:
        for name, content in files.items():
            info = tarfile.TarInfo(name)
            info.size, info.mode = len(content), (modes or {}).get(name, 0o644)
            tarred.addfile(info, io.BytesIO(content.encode()))
    data.seek(0)
    return tarfile.open(fileobj=data, mode="r:gz")


class UpdateTest(unittest.TestCase):
    def test_versions_compare_numerically(self):
        self.assertGreater(update.number("v0.6.10"), update.number("0.6.9-Alpha"))
        self.assertGreater(update.number("0.6.3"), update.number("0.6"))
        self.assertEqual(update.number("0.1.10"), update.number("0.1.10-Alpha"))

    def test_asset_matches_the_platform(self):
        assets = [{"name": "DOA3-1-Linux.tar.gz"}, {"name": "DOA3-1-macOS.tar.gz"}, {"name": "DOA3-1.zip"}]
        for suffix, expected in ((".zip", "DOA3-1.zip"), ("-macOS.tar.gz", "DOA3-1-macOS.tar.gz"),
                                 ("-Linux.tar.gz", "DOA3-1-Linux.tar.gz")):
            with mock.patch.object(update, "SUFFIX", suffix):
                self.assertEqual(update.pick_asset(assets)["name"], expected)
        with mock.patch.object(update, "SUFFIX", ".zip"), self.assertRaises(ValueError):
            update.pick_asset([{"name": "DOA3-1-Linux.tar.gz"}])

    def test_unpack_keeps_private_files_and_writes_version_last(self):
        with tempfile.TemporaryDirectory() as folder, mock.patch.object(update, "ROOT", Path(folder).resolve()):
            root = update.ROOT
            (root / "private").mkdir()
            (root / "private" / "launcher.json").write_text("mine")
            (root / "Launcher.exe").write_text("old")
            update.unpack(archive({"DOA3-2/VERSION": "2", "DOA3-2/Launcher.exe": "new", "DOA3-2/tools/a.py": "a",
                                   "DOA3-2/private/launcher.json": "release", "DOA3-2/private/photo.jpg": "photo"}))
            self.assertEqual((root / "Launcher.exe").read_text(), "new")
            self.assertEqual((root / "Launcher.exe.old").read_text(), "old")
            self.assertEqual((root / "tools" / "a.py").read_text(), "a")
            self.assertEqual((root / "private" / "launcher.json").read_text(), "mine")
            self.assertEqual((root / "private" / "photo.jpg").read_text(), "photo")
            # A failed write puts the running exe back and leaves VERSION, which goes last, for a retry.
            real_write = Path.write_bytes

            def fail_on_exe(path, data):
                if path.name == "Launcher.exe":
                    raise OSError("disk full")
                return real_write(path, data)

            with mock.patch.object(Path, "write_bytes", fail_on_exe):
                with self.assertRaises(OSError):
                    update.unpack(archive({"DOA3-3/VERSION": "3", "DOA3-3/tools/a.py": "b", "DOA3-3/Launcher.exe": "x"}))
            self.assertEqual((root / "tools" / "a.py").read_text(), "b")
            self.assertEqual((root / "VERSION").read_text(), "2")
            self.assertEqual((root / "Launcher.exe").read_text(), "new")

    def test_unpack_reads_a_tarball_and_reapplies_a_changed_mode_outside_windows(self):
        with tempfile.TemporaryDirectory() as folder, mock.patch.object(update, "ROOT", Path(folder).resolve()), \
                mock.patch.object(update.sys, "platform", "linux"), mock.patch.object(Path, "chmod") as chmod:
            (update.ROOT / "build_game.sh").write_text("same")
            update.unpack(tarball({"DOA3-2/build_game.sh": "same", "DOA3-2/VERSION": "2"},
                                  {"DOA3-2/build_game.sh": 0o755}))
            chmod.assert_any_call(0o755)
            self.assertEqual((update.ROOT / "VERSION").read_text(), "2")

    def test_unsafe_entry_stops_before_any_write(self):
        with tempfile.TemporaryDirectory() as folder, mock.patch.object(update, "ROOT", Path(folder).resolve()):
            for unsafe in (archive({"DOA3-2/tools/a.py": "a", "DOA3-2/../../evil": "x"}),
                           tarball({"DOA3-2/tools/a.py": "a", "DOA3-2/../../evil": "x"})):
                with self.assertRaises(ValueError):
                    update.unpack(unsafe)
                self.assertFalse((update.ROOT / "tools").exists())

    def test_install_refuses_a_download_without_a_matching_checksum(self):
        for asset in ({"name": "a.zip", "browser_download_url": "u"},
                      {"name": "a.zip", "browser_download_url": "u", "digest": "sha256:00"}):
            with mock.patch.object(update, "fetch", return_value=io.BytesIO(b"data")),                     mock.patch.object(update, "unpack") as unpack:
                with self.assertRaises(ValueError):
                    update.install(asset)
                unpack.assert_not_called()

    def test_unpack_skips_an_existing_private_file_behind_a_symlink(self):
        with tempfile.TemporaryDirectory() as folder, tempfile.TemporaryDirectory() as elsewhere:
            root = Path(folder).resolve()
            (Path(elsewhere) / "README.md").write_text("mine")
            try:
                (root / "private").symlink_to(elsewhere, target_is_directory=True)
            except OSError:
                self.skipTest("symlinks need privileges on this system")
            with mock.patch.object(update, "ROOT", root):
                update.unpack(archive({"DOA3-2/private/README.md": "release", "DOA3-2/VERSION": "2"}))
            self.assertEqual((Path(elsewhere) / "README.md").read_text(), "mine")
            self.assertEqual((root / "VERSION").read_text(), "2")

    def test_main_reports_a_truncated_download(self):
        with mock.patch.object(update, "installed", return_value="1"),                 mock.patch.object(update, "newer", side_effect=update.http.client.IncompleteRead(b"")),                 mock.patch("builtins.print") as shown:
            self.assertEqual(update.main(["check"]), 1)
            self.assertTrue(shown.call_args.args[0].startswith("Update failed"))


if __name__ == "__main__":
    unittest.main()
