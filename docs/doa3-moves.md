# Move data and the 3.1 balance port

Fighter move properties are disc data, not code. Each fighter has a move file
in `loadfile.afs`; the game loads it into the 96 KB window at offset
`0x200000` of that player's 4.6 MB slot (`0x00081C00` allocates the slots,
`0x004889BC + slot*0x1C` holds the window). The Japanese 3.1 disc changes
these files, so most 3.1 balance can be carried to the USA disc as data.

## Finding it

`0x000B91E0` subtracts pending damage, fighter offset `0x22`. The hit pass
`0x000B3F20` adds `round(base * multiplier)` to it: `0x000A6BF0` returns
the base and `0x000AF7F0` the counter multiplier (0.5 to 1.6).
`0x000A6BF0` reads the attacker's move file through `0x0009E9A0(player,
section)`, which returns a section pointer from the slot table.

## Move file

The file starts with 33 or 34 little-endian section offsets; the first equals
the table size (`0x84` or `0x88`). The USA disc has 20 such entries in
`loadfile.afs`, in the same order as Japan's.

Readers per section, from `0x0009E9A0` and `0x0009EA00` call sites:

| Section | Readers | Contents |
| --- | --- | --- |
| 0 | 39 | Action table, 8 bytes per action ID (fighter `0x2E`). Byte 3 is the category, byte 4 the record index in section 2, 6 or 7. |
| 2 | 26 | Strike records, 0x24 bytes. |
| 4 | 7 | Throw records, 0x20 bytes; +4 points to a per-hit damage list. |
| 6, 7 | 2 | Hold records, 0x18 bytes; +8 points to a damage list. |
| 8 | 9 | Unidentified; its readers sit between `0x000A6FE0` and `0x000A9A60`. |
| 20, 21 | 1-2 | Probably motion scripts; Japan grows section 21 by 1-2 KB per fighter. |

Strike record:

| Offset | Type | Use |
| --- | --- | --- |
| 0x00 | 12 x u16 | Reaction motion references; bits 14-15 are flags, 0xFFFF is none |
| 0x18 | u16 | Unidentified (`0x000A5F30`) |
| 0x1A | u8 | Hit property; `0x000ACB40` tests `& 3` |
| 0x1C | u16 | Unidentified; Japan shifts it by 1-2 like a reference |
| 0x1E | u8 | Unidentified (`0x000A6F50`, `0x000B5010`) |
| 0x1F | s8 | Block stun: `0x000B2100` scales the guard reaction by it |
| 0x20 | s8 | Hit stun, used the same way by `0x000B2320` |
| 0x21 | u8 | Index used by `0x0009A850` |
| 0x22 | u8 | Flag tested by `0x000AFB00` |
| 0x23 | u8 | Base damage |

Stun is relative to the reaction motion's length, so a stun value is only
meaningful together with the reactions it was tuned for.

## USA and Japan

Japan keeps every USA strike record at its index and appends new ones. It
inserts motions into the shared motion numbering, so references shift: six
motions before `0x162` move most references by 6, and some fighters have a
second insertion. The action table inserts new actions mid-table, so action
IDs differ between the versions. Three of the 20 move files have no strike
changes.

## Porting

`tools/balance31.py` compares the strike records of each pair. It learns the
motion renumbering from the records themselves and ports a record when:

- every reaction it names exists on the USA disc, and
- nothing outside reactions, block stun, hit stun and damage changed.

It writes `offset old new` hex lines against the USA `loadfile.afs`. Against
the two retail discs it ports 194 bytes in 124 records and skips 106 records:
those that use motions only Japan has, and those that change 0x18, 0x1A, 0x1C
or 0x1E. Each ported record, renumbered forward, equals its Japanese record.

```powershell
python tools/balance31.py <usa-disc> <japan-disc> tools\doa3\balance31.txt
```

The table ships as `tools/doa3/balance31.txt`, so players need only the USA
ISO. `BuildGame.cmd` and `Play.cmd` copy it to `private/play-disc` as
`doa3-31-balance.txt`, and the runner loads it from the disc root. The kernel then
filters every read of a file named `loadfile.afs`, including the game's
`Z:` cache copy, and rewrites a byte only while it still holds the USA value.
Writes to that copy get the USA values back, so the cache always matches the
disc whether or not the table is present.

A scripted Time Attack run applied the table as move files were read, reached
the expected watchdog stop, and left a cache prefix byte-identical to the USA
`loadfile.afs`. Whether each change feels right in a fight is untested.

## Not ported

- New moves: the appended strike records, the new actions and the motions
  they play exist only on the Japanese disc.
- Throws, holds and section 8.
- Section 21 and anything else that sets startup and recovery frames.
- Free-step tracking, AI and other changes in the executable.

The table holds byte values taken from the Japanese disc. Shipping it is a
named exception to the redistribution boundary
([doaxbv-re#2](https://github.com/NoRain211/doaxbv-re/issues/2)); see hard
rule 2 in `AGENTS.md`.
