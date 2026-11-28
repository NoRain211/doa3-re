# DOA3 save-folder costumes

How the game finds, lists and loads the costumes in its title save folder
(`TDATA/54430001`), and what limits them. Addresses are USA 3.0 first, then
Japan 3.1. Europe 3.2 reads a different title ID and gets no mod costumes.

## Limits

- **7 stored per character.** The scan keeps a character's first 7 distinct
  costume numbers, in folder order, and drops the rest.
- **9 menu rows per character.** The menu total is
  `min(unlocked, disc rows) + min(save costumes, 9 - disc rows)`. Disc rows
  are 3 for Zack, Kasumi and `kte`, and 2 for everyone else, so 6 or 7 save
  costumes show.
- **256 file names.** The scan stores every matching name, 16 bytes each, in
  one table that `afs_load` indexes: at most 128 costumes in total.

The runtime lists folders in NTFS order (digits before letters), so a pack's
`kas17` comes before the Booster Disc's `kasA0` and takes its slot first.
`tools/build_game.py` (`COSTUME_SLOTS`) fills only the room each menu has.

## The scan

USA `0x80a00` (cdecl, no arguments), Japan `0x86ad0` (no arguments). It runs
once at boot. For each of 19 character prefixes (`0x309a98` / `0x302c58`;
entries 12 and 16 are skipped), it searches `T:\<prefix>??.*`, lowercases each
name in the find data (`finddata + 0x2c`, helper `0x197f40` / `0x1c07c0`), and
stores it in the name table. A costume's slot key is the two hex digits after
the prefix (`%2X`: `kas17` is 0x17, `kasA0` is 0xA0). A key needs both its
`.xpr` and `.cat` to count.

| | USA | Japan |
|---|---|---|
| Slot table, 19 × 7 dwords (low word `.cat`, high word `.xpr`, each `0x8000 \| name index`) | `0x4a0df0` | `0x5b1e58` |
| Name table, 256 × 16 bytes | `0x4a10d8` | `0x5b2140` |
| Name count (low word) | `0x4a10c4` | `0x5b212c` |
| Per-character count, 19 bytes | `0x4a210c` | `0x5b3174` |
| Per-character scratch, 7 × 6 bytes | `0x4a20e0` | `0x5b3148` |
| FindFirst / FindNext / FindClose | `0x1633b7` / `0x1634c4` / `0x162caa` | `0x18b090` / `0x18b19d` / `0x18b726` |
| sprintf | `0x18dad6` | `0x1b7150` |

## Costume IDs

A costume ID is a byte. IDs `0x00`–`0x3f` are disc costumes (table `0x308770` /
`0x301640`, `(id × 25 + character) × 10` bytes). IDs from `0x40` are save
costumes, `0x40 + slot`. Only the model loader and the select data treat the
two ranges differently. The select menu's maths is signed-byte, so IDs must
stay below `0x80`: up to 64 save costumes per character.

## Select menu

The select index (0–24) maps to a prefix index through `0x309cb8` / `0x2ff868`.
Per select index: disc rows `0x36806c` / `0x29c70c`, rows per disc group
`0x3680f0` / `0x29c790`, disc IDs `0x368088` / `0x29c728`, disc ID count
`0x367fc4` / `0x29c660`. Unlocked disc rows per prefix: `0x487806` / `0x59fb16`.

| Function | USA (cdecl) | Japan |
|---|---|---|
| Menu total (the cap of 9) | `0xc8140 (select)` | `0xcd7c0`: select in `AL`, result in `AL` |
| Row to costume ID | `0xc81b0 (select, row, variant)` | `0xcd870`: select `AL`, row `DL`, variant on the stack |
| Costume ID to row | `0xc8290 (select, id, *variant)` | `0xcd900`: select `CL`, id and pointer on the stack |
| Save count / save ID accessors | `0x7fb70`, `0x7fb90` | inlined |
| Row cycle, Up/Down (clamps its step to ±1) | `0xc6780 (ctx, player, step)` | `0xccb80` |
| Costume list input (state handler) | `0xc7830` | not yet found |
| Costume list draw | `0xca540 (ctx, ...)` | `0xd18a0`: context in `EDX` |

The player state keeps the active selection in byte `+0x16` and each
selection's row in byte `[+0x16]`, so rows are signed bytes. The menu input
word is `0x86132c + player × 8` (bit 0 Up, bit 1 Down, bits 2 and 3 Left and
Right). The costume list uses only Up/Down plus confirm and back; Left/Right
move the character grid's cursor (`0xc7420`).

The draw shows one 20-pixel row per costume (`row × 0x14`) inside the
portrait panel, labelled "COS." and one digit sprite, `(row + 1) % 10`. Nine
rows fill the panel. It calls `rand()` (`0x18db30` / `0x1b8b98`) four times per
row, so the rows drawn affect the game's random numbers.

## Model loading

`costume_model_load` (USA `0x7e9c0`, cdecl `(select, id, player)`; Japan
`0x86060`: id in `AL`, select in `CL`, player on the stack) reads the disc
table for IDs below `0x40`, and otherwise `slot table[prefix × 7 + id - 0x40]`.
It hands both file IDs to the loader pointer (`0x4a10c8` / `0x5b2130`).

`afs_load` (USA `0x7e720`, cdecl `(file id, buffer)`; Japan `0x85c40`: file
ID in `AX`, buffer on the stack) opens `T:\<name table[id & 0x7fff]>` when bit
15 is set, and otherwise the disc archive entry.
