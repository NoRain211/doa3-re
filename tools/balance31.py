# SPDX-License-Identifier: GPL-3.0-or-later
"""Write the 3.1 strike balance for the USA disc from user-owned USA and Japan discs.

Each fighter's move file is a loadfile.afs entry that starts with a table of
section offsets. Section 2 holds 0x24-byte strike records; Japan appends new
records and renumbers motion references. A record is ported when every
reaction motion it names also exists on the USA disc and only its reactions,
block stun (0x1f), hit stun (0x20) and damage (0x23) changed. See
docs/doa3-moves.md.
"""
import argparse
from collections import Counter, defaultdict
from pathlib import Path
import struct

from extract_iso import sha256

USA_XBE = "e523dd15d0765899a43a1824c023e0219d129e9a8b41e8213e4e645755d2bfa3"
JAPAN_XBE = "469cb086048020d04fbc45b05fd5c470b479f06013d77d736c0ea11df5de85df"
MOVE_FILE_MAX = 0x18000  # the window the game loads a move file into
STRIKES, RECORD = 2, 0x24
REFS = range(0, 0x18, 2)
PORTED = {0x1F, 0x20, 0x23}


def afs_entries(path):
    with open(path, "rb") as afs:
        magic, count = struct.unpack("<4sI", afs.read(8))
        if magic != b"AFS\0":
            raise ValueError(f"{path} is not an AFS archive")
        table = afs.read(8 * count)
    return [struct.unpack_from("<II", table, 8 * i) for i in range(count)]


def section_offsets(blob):
    if len(blob) < 4:
        return None
    first, = struct.unpack_from("<I", blob, 0)
    if first not in (0x84, 0x88) or len(blob) < first:
        return None
    offsets = struct.unpack_from(f"<{first // 4}I", blob, 0)
    return offsets if all(o == 0 or first <= o < len(blob) for o in offsets) else None


def move_files(path):
    files = []
    with open(path, "rb") as afs:
        for offset, size in afs_entries(path):
            if 0x100 <= size <= MOVE_FILE_MAX:
                afs.seek(offset)
                blob = afs.read(size)
                if section_offsets(blob):
                    files.append((offset, blob))
    return files


def section(blob, number):
    offsets = section_offsets(blob)
    start = offsets[number]
    end = min([o for o in offsets if o > start] + [len(blob)])
    return start, blob[start:end]


def refs(record):
    return struct.unpack_from("<12H", record, 0)


def strike_patches(usa, japan):
    """Returns (offset in the USA file, old, new) and the skipped record count."""
    start, a = section(usa, STRIKES)
    _, b = section(japan, STRIKES)
    count = min(len(a), len(b)) // RECORD
    pairs = [(a[i * RECORD:(i + 1) * RECORD], b[i * RECORD:(i + 1) * RECORD]) for i in range(count)]
    votes = defaultdict(Counter)
    for ra, rb in pairs:
        for u, v in zip(refs(ra), refs(rb)):
            if u != 0xFFFF and v != 0xFFFF and u & 0xC000 == v & 0xC000:
                votes[u & 0x3FFF][v & 0x3FFF] += 1
    renumber = {u: c.most_common(1)[0][0] for u, c in votes.items()}
    targets = Counter(renumber.values())
    # A Japan motion maps back only when exactly one USA motion became it.
    usa_motion = {v: u for u, v in renumber.items() if targets[v] == 1}

    def to_usa(v):
        if v == 0xFFFF:
            return v
        u = usa_motion.get(v & 0x3FFF)
        return None if u is None else v & 0xC000 | u

    patches, skipped = [], 0
    for i, (ra, rb) in enumerate(pairs):
        wanted = [to_usa(v) for v in refs(rb)]
        changed = [j for j in range(len(REFS) * 2, RECORD) if ra[j] != rb[j]]
        if not changed and list(refs(ra)) == wanted:
            continue
        if None in wanted or not set(changed) <= PORTED:
            skipped += 1
            continue
        new = bytearray(ra)
        struct.pack_into("<12H", new, 0, *wanted)
        for j in changed:
            new[j] = rb[j]
        patches.extend((start + i * RECORD + j, ra[j], new[j]) for j in range(RECORD) if ra[j] != new[j])
    return patches, skipped


def build(usa_disc, japan_disc):
    for disc, expected in ((usa_disc, USA_XBE), (japan_disc, JAPAN_XBE)):
        if sha256(Path(disc) / "default.xbe") != expected:
            raise ValueError(f"{disc} is not the expected Dead or Alive 3 disc")
    usa = move_files(Path(usa_disc) / "loadfile.afs")
    japan = move_files(Path(japan_disc) / "loadfile.afs")
    if len(usa) != len(japan):
        raise ValueError(f"move file counts differ: {len(usa)} and {len(japan)}")
    lines, total_skipped = [], 0
    for (offset, a), (_, b) in zip(usa, japan):
        patches, skipped = strike_patches(a, b)
        total_skipped += skipped
        lines.extend(f"{offset + o:08x} {old:02x} {new:02x}" for o, old, new in patches)
    return lines, total_skipped


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("usa_disc", type=Path)
    parser.add_argument("japan_disc", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    lines, skipped = build(args.usa_disc, args.japan_disc)
    args.output.write_text(
        "# DOA3 3.1 strike balance for the USA loadfile.afs: offset old new\n"
        + "".join(line + "\n" for line in lines), encoding="ascii")
    print(f"Wrote {len(lines)} byte changes to {args.output}; "
          f"skipped {skipped} reworked strike records")


if __name__ == "__main__":
    main()
