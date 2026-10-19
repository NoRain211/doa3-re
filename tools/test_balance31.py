import struct
import unittest

from balance31 import RECORD, section_offsets, strike_patches


def move_file(records):
    table = [0] * 34
    table[0] = 0x88
    table[2] = 0x88
    return struct.pack("<34I", *table) + b"".join(records)


def record(refs, guard=0, hit=0, damage=0, level=2):
    body = struct.pack("<12H", *refs) + bytes([0, 0, level, 0, 0, 0, 0, guard, hit, 0, 0, damage])
    assert len(body) == RECORD
    return body


class StrikeBalanceTest(unittest.TestCase):
    def test_ports_values_and_reactions_that_exist_on_the_usa_disc(self):
        refs = [0x4010, 0x4162, 0x4170] + [0xFFFF] * 9
        moved = [0x4010, 0x4168, 0x4176] + [0xFFFF] * 9  # Japan inserts six motions before 0x162
        usa = move_file([record(refs, guard=12, damage=30),
                         record(refs, guard=5),
                         record(refs, damage=20, level=2),
                         record(refs, guard=4)])
        japan = move_file([record(moved, guard=5, damage=28),
                           record([0x4163] + moved[1:], guard=2),
                           record(moved, damage=20, level=3),
                           record([0x4176, 0x4168, 0x4010] + moved[3:], guard=3),
                           record(moved, damage=40)])
        self.assertIsNotNone(section_offsets(usa))
        patches, skipped = strike_patches(usa, japan)
        swapped = 0x88 + 3 * RECORD
        self.assertEqual(patches, [
            (0x88 + 0x1F, 12, 5), (0x88 + 0x23, 30, 28),
            (swapped, 0x10, 0x70), (swapped + 1, 0x40, 0x41),
            (swapped + 4, 0x70, 0x10), (swapped + 5, 0x41, 0x40), (swapped + 0x1F, 4, 3)])
        # 0x4163 is a motion only Japan has; level (0x1a) is not understood yet.
        self.assertEqual(skipped, 2)


if __name__ == "__main__":
    unittest.main()
