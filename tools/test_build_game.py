# SPDX-License-Identifier: GPL-3.0-or-later
"""Play-disc linking with and without movies, on a synthetic disc."""
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import build_game
from build_game import MOVIE_STUB, prepare_play_disc


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

            prepare_play_disc(disc, movies=False)
            self.assertEqual((play / "mv_st.sfd").read_bytes(), movie[:MOVIE_STUB])
            self.assertEqual((disc / "mv_st.sfd").read_bytes(), movie, "a user's folder is never cut")
            self.assertTrue(os.path.samefile(play / "default.xbe", disc / "default.xbe"))
            self.assertFalse((play / ".recomp-storage").exists())

            prepare_play_disc(disc, movies=True)
            self.assertTrue(os.path.samefile(play / "mv_st.sfd", disc / "mv_st.sfd"))

            # An extraction the build owns is cut in place, with its play-disc link.
            owned = root / "private" / "imported-disc" / "disc"
            owned.parent.mkdir(parents=True)
            disc.rename(owned)
            prepare_play_disc(owned, movies=False)
            self.assertEqual((owned / "mv_st.sfd").stat().st_size, MOVIE_STUB)
            self.assertTrue(build_game.movies_cut(owned))
            self.assertEqual((play / "mv_st.sfd").read_bytes(), movie[:MOVIE_STUB])


if __name__ == "__main__":
    unittest.main()
