# SPDX-License-Identifier: GPL-3.0-or-later
"""Play-disc linking with and without movies, on a synthetic disc."""
import hashlib
import http.client
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import zipfile

import build_game
from build_game import (MOVIE_STUB, drop_owned_extraction, find_disc, installed_versions, prepare_play_disc,
                        record_install, runner_dir)
from extract_iso import sha256


def sha256_of(data):
    return hashlib.sha256(data).hexdigest()


def fake_extract(iso, output, tool, iso_sha256):
    """extract-xiso stand-in: the ISO's bytes become default.xbe."""
    assert iso_sha256 == sha256(iso), "find_disc hands extract the ISO's hash"
    (output / "disc").mkdir(parents=True)
    (output / "disc" / "default.xbe").write_bytes(iso.read_bytes())
    (output / "receipt.json").write_text(f'{{"iso_sha256": "{sha256(iso)}"}}', encoding="utf-8")


def staging_locked(source, target):
    """rename_retrying stand-in: a scan holds the fresh extraction past every retry."""
    if source.name == "imported-disc.new":
        raise PermissionError("locked by a scan")
    return source.rename(target)


class PlayDiscTests(unittest.TestCase):
    def test_movies_are_cut_and_restored(self):
        with tempfile.TemporaryDirectory() as folder, patch.object(build_game, "ROOT", Path(folder)):
            root = Path(folder)
            disc, play = root / "user disc", root / "private" / "play-disc"
            (disc / ".recomp-storage").mkdir(parents=True)
            (disc / ".recomp-storage" / "save").write_bytes(b"save")
            (disc / "default.xbe").write_bytes(b"xbe")
            movie = bytes(range(256)) * 64
            (disc / "mv_st.sfd").write_bytes(movie)
            (disc / "short.sfd").write_bytes(b"tiny")

            prepare_play_disc(disc, movies=False)
            self.assertEqual((play / "mv_st.sfd").read_bytes(), movie[:MOVIE_STUB])
            self.assertEqual((disc / "mv_st.sfd").read_bytes(), movie, "a user's folder is never cut")
            self.assertEqual((play / "short.sfd").read_bytes(), b"tiny", "a short file is never padded")
            self.assertTrue(os.path.samefile(play / "default.xbe", disc / "default.xbe"))
            self.assertFalse((play / ".recomp-storage").exists())
            self.assertEqual(drop_owned_extraction(disc, play), disc, "a user's folder is never dropped")
            self.assertEqual((disc / "mv_st.sfd").read_bytes(), movie)

            prepare_play_disc(disc, movies=True)
            self.assertTrue(os.path.samefile(play / "mv_st.sfd", disc / "mv_st.sfd"))

            # An extraction the build owns stays whole until the build succeeds.
            owned = root / "private" / "imported-disc" / "disc"
            owned.parent.mkdir(parents=True)
            disc.rename(owned)
            prepare_play_disc(owned, movies=False)
            self.assertEqual((owned / "mv_st.sfd").read_bytes(), movie)
            self.assertEqual((play / "mv_st.sfd").read_bytes(), movie[:MOVIE_STUB])
            # Saves left in an extraction by an older build keep it.
            self.assertEqual(drop_owned_extraction(owned, play), owned)
            self.assertEqual((owned / ".recomp-storage" / "save").read_bytes(), b"save")
            build_game.shutil.rmtree(owned / ".recomp-storage")
            # A folder that stays locked, or a disc.old that cannot be cleared, keeps
            # the extraction instead of failing the finished build.
            with patch.object(build_game, "rename_retrying", side_effect=PermissionError("locked")), \
                    patch("builtins.print"):
                self.assertEqual(drop_owned_extraction(owned, play), owned)
            self.assertEqual((owned / "mv_st.sfd").read_bytes(), movie)
            self.assertEqual(drop_owned_extraction(owned, play), play)
            self.assertFalse(owned.exists())
            self.assertEqual((play / "mv_st.sfd").read_bytes(), movie[:MOVIE_STUB])
            self.assertEqual((play / "default.xbe").read_bytes(), b"xbe")

    def test_dropped_extraction_is_reused_from_the_play_folder(self):
        with tempfile.TemporaryDirectory() as folder, patch.object(build_game, "ROOT", Path(folder)),                 patch.object(build_game, "extract", side_effect=fake_extract),                 patch.object(build_game, "VERSIONS", {sha256_of(b"iso"): {"suffix": "-jp"}}):
            private = Path(folder) / "private"
            imported, play = private / "imported-disc-jp", private / "play-disc-jp"
            iso = Path(folder) / "doa3.iso"
            iso.write_bytes(b"iso")
            disc = find_disc(iso)
            prepare_play_disc(disc, movies=True, name="play-disc-jp")
            self.assertEqual(drop_owned_extraction(disc, play), play)
            self.assertTrue((imported / "receipt.json").is_file())

            with patch.object(build_game, "extract") as extract:
                self.assertEqual(find_disc(iso), play)
            extract.assert_not_called()
            self.assertEqual(build_game.rebuild_disc(), play)
            # Without the play folder the ISO is extracted again.
            build_game.shutil.rmtree(play)
            self.assertIsNone(build_game.rebuild_disc())
            self.assertEqual(find_disc(iso), imported / "disc")

    def test_cut_extraction_is_rebuilt_or_restored(self):
        with tempfile.TemporaryDirectory() as folder, patch.object(build_game, "ROOT", Path(folder)):
            imported = Path(folder) / "private" / "imported-disc"
            (imported / "disc").mkdir(parents=True)
            (imported / "disc" / "mv_st.sfd").write_bytes(bytes(MOVIE_STUB))
            iso = Path(folder) / "doa3.iso"
            iso.write_bytes(b"iso")
            (imported / "receipt.json").write_text(f'{{"iso_sha256": "{sha256(iso)}"}}', encoding="utf-8")

            # Without an ISO a rebuild keeps the movies out instead of failing.
            self.assertFalse(build_game.ask_movies(None))
            self.assertEqual(find_disc(None, movies=False), imported / "disc")
            with self.assertRaisesRegex(ValueError, "restore them"):
                find_disc(None, movies=True)
            # The same ISO with movies re-extracts.
            with patch.object(build_game, "extract", side_effect=fake_extract), \
                    patch.object(build_game, "VERSIONS", {sha256_of(b"iso"): {"suffix": ""}}):
                self.assertEqual(find_disc(iso, movies=True), imported / "disc")
            self.assertEqual((imported / "disc" / "default.xbe").read_bytes(), b"iso")
            self.assertFalse(build_game.movies_cut(imported / "disc"))

    def test_each_version_keeps_its_own_extraction(self):
        with tempfile.TemporaryDirectory() as folder, patch.object(build_game, "ROOT", Path(folder)), \
                patch.object(build_game, "extract", side_effect=fake_extract), \
                patch.object(build_game, "VERSIONS", {sha256_of(b"jp"): {"suffix": "-jp"},
                                                      sha256_of(b"usa"): {"suffix": ""},
                                                      sha256_of(b"eur"): {"suffix": "-eur"}}):
            private = Path(folder) / "private"
            usa = private / "imported-disc"
            (usa / "disc").mkdir(parents=True)
            (usa / "disc" / "default.xbe").write_bytes(b"usa")
            (usa / "receipt.json").write_text('{"iso_sha256": "other"}', encoding="utf-8")
            iso = Path(folder) / "jp.iso"
            iso.write_bytes(b"jp")
            eur_iso = Path(folder) / "eur.iso"
            eur_iso.write_bytes(b"eur")

            self.assertEqual(find_disc(iso), private / "imported-disc-jp" / "disc")
            self.assertEqual(find_disc(eur_iso), private / "imported-disc-eur" / "disc")
            self.assertEqual((usa / "disc" / "default.xbe").read_bytes(), b"usa", "USA extraction untouched")
            self.assertEqual((private / "imported-disc-jp" / "disc" / "default.xbe").read_bytes(), b"jp",
                             "Japan extraction untouched")
            self.assertFalse((private / "imported-disc.new").exists())
            # The same ISO again reuses its extraction.
            with patch.object(build_game, "extract") as extract:
                self.assertEqual(find_disc(iso), private / "imported-disc-jp" / "disc")
                self.assertEqual(find_disc(eur_iso), private / "imported-disc-eur" / "disc")
            extract.assert_not_called()
            with self.assertRaisesRegex(ValueError, "USA 3.0 and Europe 3.2 only"):
                build_game.disc_version(Path(folder))

    def test_different_iso_replaces_the_import(self):
        with tempfile.TemporaryDirectory() as folder, patch.object(build_game, "ROOT", Path(folder)), \
                patch.object(build_game, "extract", side_effect=fake_extract), \
                patch.object(build_game, "VERSIONS", {sha256_of(b"first"): {"suffix": ""},
                                                      sha256_of(b"second"): {"suffix": ""}}):
            private = Path(folder) / "private"
            imported = private / "imported-disc"
            iso = Path(folder) / "doa3.iso"
            iso.write_bytes(b"first")
            self.assertEqual(find_disc(iso), imported / "disc")

            iso.write_bytes(b"second")
            self.assertEqual(find_disc(iso), imported / "disc")
            self.assertEqual((imported / "disc" / "default.xbe").read_bytes(), b"second")
            self.assertEqual(json.loads((imported / "receipt.json").read_text())["iso_sha256"],
                             sha256_of(b"second"))
            self.assertFalse((private / "imported-disc.new").exists())

    def test_unsupported_iso_keeps_the_import(self):
        with tempfile.TemporaryDirectory() as folder, patch.object(build_game, "ROOT", Path(folder)), \
                patch.object(build_game, "extract", side_effect=fake_extract), \
                patch.object(build_game, "VERSIONS", {sha256_of(b"first"): {"suffix": ""}}):
            private = Path(folder) / "private"
            imported = private / "imported-disc"
            iso = Path(folder) / "doa3.iso"
            iso.write_bytes(b"first")
            self.assertEqual(find_disc(iso), imported / "disc")

            iso.write_bytes(b"unwanted")
            with self.assertRaisesRegex(ValueError, "USA 3.0 and Europe 3.2 only"):
                find_disc(iso)
            self.assertFalse((private / "imported-disc.new").exists())
            self.assertEqual((imported / "disc" / "default.xbe").read_bytes(), b"first")
            self.assertEqual(json.loads((imported / "receipt.json").read_text())["iso_sha256"],
                             sha256_of(b"first"))

    def test_failed_extraction_keeps_the_import(self):
        def broken(iso, output, tool, iso_sha256):
            (output / "disc").mkdir(parents=True)
            raise OSError("unreadable image")

        with tempfile.TemporaryDirectory() as folder, patch.object(build_game, "ROOT", Path(folder)), \
                patch.object(build_game, "extract", side_effect=fake_extract), \
                patch.object(build_game, "VERSIONS", {sha256_of(b"first"): {"suffix": ""}}):
            private = Path(folder) / "private"
            imported = private / "imported-disc"
            iso = Path(folder) / "doa3.iso"
            iso.write_bytes(b"first")
            self.assertEqual(find_disc(iso), imported / "disc")

            iso.write_bytes(b"broken")
            with patch.object(build_game, "extract", side_effect=broken):
                with self.assertRaisesRegex(OSError, "unreadable image"):
                    find_disc(iso)
            self.assertFalse((private / "imported-disc.new").exists())
            self.assertEqual((imported / "disc" / "default.xbe").read_bytes(), b"first")
            self.assertEqual(json.loads((imported / "receipt.json").read_text())["iso_sha256"],
                             sha256_of(b"first"))

    def test_failed_rename_restores_the_import(self):
        with tempfile.TemporaryDirectory() as folder, patch.object(build_game, "ROOT", Path(folder)), \
                patch.object(build_game, "extract", side_effect=fake_extract), \
                patch.object(build_game, "VERSIONS", {sha256_of(b"first"): {"suffix": ""},
                                                      sha256_of(b"second"): {"suffix": ""}}):
            private = Path(folder) / "private"
            imported = private / "imported-disc"
            iso = Path(folder) / "doa3.iso"
            iso.write_bytes(b"first")
            self.assertEqual(find_disc(iso), imported / "disc")

            iso.write_bytes(b"second")
            with patch.object(build_game, "rename_retrying", side_effect=staging_locked):
                with self.assertRaisesRegex(PermissionError, "locked by a scan"):
                    find_disc(iso)
            self.assertFalse((private / "imported-disc.new").exists())
            self.assertFalse((private / "imported-disc.old").exists())
            self.assertEqual((imported / "disc" / "default.xbe").read_bytes(), b"first")

    def test_leftover_import_is_put_back(self):
        # A run that died between the renames left the only import at imported-disc.old.
        with tempfile.TemporaryDirectory() as folder, patch.object(build_game, "ROOT", Path(folder)), \
                patch.object(build_game, "extract", side_effect=fake_extract), \
                patch.object(build_game, "VERSIONS", {sha256_of(b"first"): {"suffix": ""},
                                                      sha256_of(b"second"): {"suffix": ""}}):
            private = Path(folder) / "private"
            imported = private / "imported-disc"
            iso = Path(folder) / "doa3.iso"
            iso.write_bytes(b"first")
            find_disc(iso)
            imported.rename(private / "imported-disc.old")

            iso.write_bytes(b"second")
            with patch.object(build_game, "rename_retrying", side_effect=staging_locked):
                with self.assertRaisesRegex(PermissionError, "locked by a scan"):
                    find_disc(iso)
            self.assertEqual((imported / "disc" / "default.xbe").read_bytes(), b"first")
            self.assertFalse((private / "imported-disc.old").exists())

    def test_locked_staging_keeps_the_real_error(self):
        real_rmtree = build_game.shutil.rmtree

        def locked(path, ignore_errors=False):
            if Path(path).name == "imported-disc.new" and not ignore_errors:
                raise PermissionError("staging locked")
            return real_rmtree(path, ignore_errors=ignore_errors)

        with tempfile.TemporaryDirectory() as folder, patch.object(build_game, "ROOT", Path(folder)), \
                patch.object(build_game, "extract", side_effect=fake_extract), \
                patch.object(build_game, "VERSIONS", {}), \
                patch.object(build_game.shutil, "rmtree", side_effect=locked):
            iso = Path(folder) / "doa3.iso"
            iso.write_bytes(b"unwanted")
            with self.assertRaisesRegex(ValueError, "USA 3.0 and Europe 3.2 only"):
                find_disc(iso)

    def test_closed_stdin_installs_movies(self):
        with tempfile.TemporaryDirectory() as folder, patch.object(build_game, "ROOT", Path(folder)), \
                patch("sys.stdin.isatty", return_value=True), patch("builtins.input", side_effect=EOFError):
            self.assertTrue(build_game.ask_movies(None))

    def test_locked_play_disc_rename_is_retried(self):
        with tempfile.TemporaryDirectory() as folder, patch.object(build_game, "ROOT", Path(folder)), \
                patch.object(build_game.time, "sleep") as sleep:
            disc = Path(folder) / "disc"
            disc.mkdir()
            (disc / "default.xbe").write_bytes(b"xbe")
            rename = Path.rename
            calls = []

            def locked_once(self, target):
                calls.append(self)
                if len(calls) == 1:
                    raise PermissionError("scanning")
                return rename(self, target)

            with patch.object(Path, "rename", locked_once):
                prepare_play_disc(disc, movies=True)
            self.assertEqual(len(calls), 2)
            sleep.assert_called_once()
            self.assertEqual((Path(folder) / "private" / "play-disc" / "default.xbe").read_bytes(), b"xbe")


class BuildRecordTests(unittest.TestCase):
    def test_each_version_keeps_its_own_runner_folder(self):
        with tempfile.TemporaryDirectory() as folder, patch.object(build_game, "ROOT", Path(folder)):
            root = Path(folder)
            self.assertEqual(runner_dir({"suffix": ""}), root / "build" / "recomp-program")
            self.assertEqual(runner_dir({"suffix": "-jp"}), root / "build" / "recomp-program-jp")
            self.assertEqual(runner_dir({"suffix": "-eur"}), root / "build" / "recomp-program-eur")

    def test_installed_list_keeps_every_version_most_recent_last(self):
        with tempfile.TemporaryDirectory() as folder, patch.object(build_game, "ROOT", Path(folder)):
            root = Path(folder)
            (root / "private").mkdir()
            jp = runner_dir({"suffix": "-jp"})
            usa = runner_dir({"suffix": ""})
            record_install("Japan 3.1", "play-disc-jp", "default.xbe", jp)
            record = json.loads((root / "private" / "build.json").read_text(encoding="utf-8"))
            self.assertEqual((record["version"], record["play_disc"], record["xbe"]),
                             ("Japan 3.1", "play-disc-jp", "default.xbe"), "the top-level keys stay the last build")
            self.assertEqual(record["installed"], [{"version": "Japan 3.1", "play_disc": "play-disc-jp",
                                                    "xbe": "default.xbe", "build_dir": "build/recomp-program-jp"}])

            record_install("USA 3.0", "play-disc", "default.xbe", usa)
            record = json.loads((root / "private" / "build.json").read_text(encoding="utf-8"))
            self.assertEqual(record["version"], "USA 3.0")
            self.assertEqual([each["version"] for each in record["installed"]], ["Japan 3.1", "USA 3.0"])
            self.assertEqual([each["build_dir"] for each in record["installed"]],
                             ["build/recomp-program-jp", "build/recomp-program"])

            # Rebuilding a version updates its entry and moves it to the end, without
            # replacing the others.
            record_install("Japan 3.1", "play-disc-jp", "default.xbe", jp)
            record = json.loads((root / "private" / "build.json").read_text(encoding="utf-8"))
            self.assertEqual([each["version"] for each in record["installed"]], ["USA 3.0", "Japan 3.1"])

    def test_old_record_becomes_the_first_install(self):
        with tempfile.TemporaryDirectory() as folder, patch.object(build_game, "ROOT", Path(folder)):
            root = Path(folder)
            (root / "private").mkdir()
            (root / "private" / "build.json").write_text(
                json.dumps({"version": "USA 3.0", "play_disc": "play-disc", "xbe": "default.xbe"}), encoding="utf-8")
            record_install("Japan 3.1", "play-disc-jp", "default.xbe", runner_dir({"suffix": "-jp"}))
            record = json.loads((root / "private" / "build.json").read_text(encoding="utf-8"))
            self.assertEqual([(each["version"], each["build_dir"]) for each in record["installed"]],
                             [("USA 3.0", "build/recomp-program"), ("Japan 3.1", "build/recomp-program-jp")])

    def test_a_build_drops_the_install_whose_runner_it_replaced(self):
        # Before the list, Japan 3.1 built into build/recomp-program, which is USA's
        # folder now; a USA build replaces that runner, so Japan must not stay listed there.
        with tempfile.TemporaryDirectory() as folder, patch.object(build_game, "ROOT", Path(folder)):
            root = Path(folder)
            (root / "private").mkdir()
            (root / "private" / "build.json").write_text(
                json.dumps({"version": "Japan 3.1", "play_disc": "play-disc-jp", "xbe": "default.xbe"}), encoding="utf-8")
            record_install("USA 3.0", "play-disc", "default.xbe", runner_dir({"suffix": ""}))
            record = json.loads((root / "private" / "build.json").read_text(encoding="utf-8"))
            self.assertEqual([(each["version"], each["build_dir"]) for each in record["installed"]],
                             [("USA 3.0", "build/recomp-program")])

    def test_unreadable_record_starts_over(self):
        with tempfile.TemporaryDirectory() as folder, patch.object(build_game, "ROOT", Path(folder)):
            root = Path(folder)
            (root / "private").mkdir()
            (root / "private" / "build.json").write_text("{not json", encoding="utf-8")
            record_install("USA 3.0", "play-disc", "default.xbe", runner_dir({"suffix": ""}))
            record = json.loads((root / "private" / "build.json").read_text(encoding="utf-8"))
            self.assertEqual([each["version"] for each in record["installed"]], ["USA 3.0"])

    def test_a_record_without_a_list_is_one_install(self):
        self.assertEqual(installed_versions({}), [{"version": "USA 3.0", "play_disc": "play-disc",
                                                   "xbe": "default.xbe", "build_dir": "build/recomp-program"}])
        self.assertEqual(installed_versions({"version": "Japan 3.1", "play_disc": "play-disc-jp"}),
                         [{"version": "Japan 3.1", "play_disc": "play-disc-jp",
                           "xbe": "default.xbe", "build_dir": "build/recomp-program"}])
        listed = [{"version": "USA 3.0", "play_disc": "play-disc", "xbe": "default.xbe",
                   "build_dir": "build/recomp-program"}]
        self.assertEqual(installed_versions({"version": "USA 3.0", "installed": listed}), listed)


class BuildJobsTests(unittest.TestCase):
    def test_one_job_per_4_gb_and_at_least_4(self):
        for cores, gigabytes, jobs in ((16, 64, 16), (16, 32, 8), (16, 16, 4), (16, 0, 4), (2, 64, 2), (8, 64, 8)):
            with self.subTest(cores=cores, gigabytes=gigabytes), patch.object(os, "cpu_count", return_value=cores),                     patch.object(build_game, "physical_memory", return_value=gigabytes << 30):
                self.assertEqual(build_game.build_jobs(), jobs)


class WindowsToolsTests(unittest.TestCase):
    def test_download_is_verified_and_unpacked_once(self):
        with tempfile.TemporaryDirectory() as folder:
            with zipfile.ZipFile(Path(folder) / "tool.zip", "w") as contents:
                contents.writestr("tool-1.0-x86_64/bin/tool.exe", b"tool")
            zipped = (Path(folder) / "tool.zip").read_bytes()

        def fake_download(url, path):
            Path(path).write_bytes(zipped)

        with tempfile.TemporaryDirectory() as folder, patch.object(build_game, "ROOT", Path(folder)), \
                patch("urllib.request.urlretrieve", side_effect=fake_download) as download, \
                patch("builtins.print"):
            private = Path(folder) / "private"
            home = private / "tool-1.0"
            with patch.object(build_game, "WINDOWS_TOOLS", [("tool-1.0", "tool.exe", "https://x/tool.zip", "0" * 64)]), \
                    self.assertRaisesRegex(ValueError, "pinned SHA-256"):
                build_game.windows_tools()
            self.assertEqual(list(private.iterdir()), [], "a rejected download leaves nothing")
            with patch.object(build_game, "WINDOWS_TOOLS",
                              [("tool-1.0", "tool.exe", "https://x/tool.zip", sha256_of(zipped))]):
                self.assertEqual(build_game.windows_tools(), [home / "bin"])
                self.assertEqual((home / "bin" / "tool.exe").read_bytes(), b"tool")
                self.assertEqual(list(private.iterdir()), [home])
                self.assertEqual(build_game.windows_tools(), [home / "bin"])
            self.assertEqual(download.call_count, 2, "an unpacked toolchain is reused")


class CostumeTests(unittest.TestCase):
    def test_characters_take_turns_within_the_budget(self):
        offered = [(name, name) for name in ["aya05", "aya06", "aya07", "kas17", "zac04"]]
        with patch.object(build_game, "COSTUME_FILES", 12):
            # The 4 files of kasA0 and aya05 leave room for 4 costumes.
            picked = build_game.pick_costumes(offered, {"kasA0", "aya05"})
        self.assertEqual([name for name, _ in picked], ["aya06", "kas17", "zac04", "aya07"])

    def test_packs_are_installed_once_and_a_bad_pack_is_skipped(self):
        archive = tempfile.SpooledTemporaryFile()
        with zipfile.ZipFile(archive, "w") as contents:
            for name in ["aya05.xpr", "aya05.cat", "aya07-nobow.xpr", "aya07-nobow.cat", "kas17.xpr", "aya.jpg"]:
                contents.writestr("pack/" + name, name)
        archive.seek(0)
        zipped = archive.read()

        def fake_download(page, path):
            if page == "bad":
                raise http.client.IncompleteRead(b"")  # a dropped download, not an OSError
            path.write_bytes(zipped)

        # The bad pack comes first, so the good one must not trip over its staging folder.
        with tempfile.TemporaryDirectory() as folder, patch.object(build_game, "ROOT", Path(folder)), \
                patch.object(build_game, "COSTUME_PACKS", [("Bad", "bad", sha256_of(zipped)),
                                                           ("Good", "good", sha256_of(zipped))]), \
                patch.object(build_game, "download_mediafire", side_effect=fake_download) as download, \
                patch("builtins.print"):
            storage = Path(folder) / "private" / "play-disc" / ".recomp-storage"
            saves = storage / "partition1" / "TDATA" / "54430001"
            build_game.install_costumes("play-disc")
            # The variant is not a name the game loads, and kas17 has no .cat.
            self.assertEqual(sorted(file.name for file in saves.iterdir()), ["aya05.cat", "aya05.xpr"])
            self.assertEqual((storage / "costume-packs.txt").read_text(encoding="utf-8"), "good\n")
            self.assertFalse((Path(folder) / "private" / "costume-packs.tmp").exists())
            (saves / "aya05.xpr").unlink()
            (saves / "aya05.cat").unlink()
            build_game.install_costumes("play-disc")
            self.assertEqual(list(saves.iterdir()), [], "a deleted costume stays deleted")
            self.assertEqual([call.args[0] for call in download.call_args_list], ["bad", "good", "bad"])


if __name__ == "__main__":
    unittest.main()
