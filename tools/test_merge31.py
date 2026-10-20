import struct
import tempfile
import unittest
from pathlib import Path

from merge31 import SECTOR, merge
from balance31 import afs_entries


def move_file(tag):
    return struct.pack("<34I", *([0x88] * 34)) + bytes([tag]) * 0x100


def write_afs(path, entries):
    blob = bytearray(struct.pack("<4sI", b"AFS\0", len(entries)))
    blob += bytes(8 * len(entries) + 8)
    for i, data in enumerate(entries):
        blob += bytes(-len(blob) % SECTOR)
        struct.pack_into("<II", blob, 8 + 8 * i, len(blob), len(data))
        blob += data
    path.write_bytes(blob)


class MergeTest(unittest.TestCase):
    def test_japan_entries_take_the_usa_numbers_in_file_order(self):
        usa = [b"bank-us", b"pair-us", b"stage", move_file(1), b"motion-us", b"menu"]
        # Japan inserts an entry before the fighter and grows the motion entry.
        japan = [b"bank-jp", b"pair-jp", b"stage", b"new", move_file(2), b"motion-jp" * 900, b"menu"]
        with tempfile.TemporaryDirectory() as folder:
            folder = Path(folder)
            write_afs(folder / "us.afs", usa)
            write_afs(folder / "jp.afs", japan)
            self.assertEqual(merge(folder / "us.afs", folder / "jp.afs", folder / "out.afs"), 4)
            out = (folder / "out.afs").read_bytes()
            table = afs_entries(folder / "out.afs")
        self.assertEqual([out[o:o + s] for o, s in table],
                         [b"bank-jp", b"pair-jp", b"stage", move_file(2), b"motion-jp" * 900, b"menu"])
        # The game stalls on a table whose offsets go backwards.
        self.assertEqual(table, sorted(table))


if __name__ == "__main__":
    unittest.main()
