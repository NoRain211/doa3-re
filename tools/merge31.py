# SPDX-License-Identifier: GPL-3.0-or-later
"""Write a USA loadfile.afs that carries Japan 3.1's move and motion data.

Each fighter's move file and the motion entry after it, plus the shared motion
bank in entries 0 and 1, are taken from the Japanese disc under their USA entry
numbers, so the USA executable finds them where it expects. Entries stay in
file order. Both discs are user-owned; see docs/doa3-moves.md.
"""
import argparse
from pathlib import Path
import struct

from balance31 import JAPAN_XBE, MOVE_FILE_MAX, USA_XBE, afs_entries, section_offsets
from extract_iso import sha256

SECTOR = 0x800
SHARED = (0, 1)  # the common motion bank and its companion


def move_indices(path, entries):
    found = []
    with open(path, "rb") as afs:
        for i, (offset, size) in enumerate(entries):
            if 0x100 <= size <= MOVE_FILE_MAX:
                afs.seek(offset)
                if section_offsets(afs.read(size)):
                    found.append(i)
    return found


def swaps(usa_afs, japan_afs):
    usa, japan = afs_entries(usa_afs), afs_entries(japan_afs)
    a, b = move_indices(usa_afs, usa), move_indices(japan_afs, japan)
    if len(a) != len(b):
        raise ValueError(f"move file counts differ: {len(a)} and {len(b)}")
    pairs = list(zip(SHARED, SHARED))
    for u, j in zip(a, b):
        pairs += [(u, j), (u + 1, j + 1)]
    return usa, japan, pairs


def merge(usa_afs, japan_afs, output):
    usa, japan, pairs = swaps(usa_afs, japan_afs)
    taken = dict(pairs)
    partial = output.with_name(output.name + ".tmp")
    with open(partial, "wb") as out, open(usa_afs, "rb") as a, open(japan_afs, "rb") as b:
        out.write(a.read(usa[0][0]))  # header, table and padding up to the first entry
        table = []
        for i, (offset, size) in enumerate(usa):
            source = a
            if i in taken:
                source, (offset, size) = b, japan[taken[i]]
            source.seek(offset)
            table.append((out.tell(), size))
            out.write(source.read(size))
            out.write(bytes(-out.tell() % SECTOR))
        out.seek(8)
        out.write(b"".join(struct.pack("<II", *entry) for entry in table))
    partial.replace(output)
    return len(pairs)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("usa_disc", type=Path)
    parser.add_argument("japan_disc", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    for disc, expected in ((args.usa_disc, USA_XBE), (args.japan_disc, JAPAN_XBE)):
        if sha256(disc / "default.xbe") != expected:
            raise ValueError(f"{disc} is not the expected Dead or Alive 3 disc")
    count = merge(args.usa_disc / "loadfile.afs", args.japan_disc / "loadfile.afs", args.output)
    print(f"Took {count} entries from Japan into {args.output}")


if __name__ == "__main__":
    main()

