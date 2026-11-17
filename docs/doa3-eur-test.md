# Europe 3.2 (work in progress)

Bring-up of the European disc (Europe, Australia; En, Ja, Fr, De, Es, It),
default.xbe SHA-256
2d1b63b072fdefcd97174afeb28a111ce9443633f3f2a12122a8dea3fca037ab, title ID
0x4D53002D, region mask 4, built 2002-02-14. Every statically linked library
(XAPILIB, D3D8, D3DX8, XGRAPHC, DSOUND, XBOXKRNL, LIBCMT, LIBCPMT) is XDK
4134, the same as Japan 3.1, so the Japan recipe is the starting point, not
USA.

## Step 1: map Japan to Europe without a Japan disc

The Europe disc is the only input. The Japan recipe ships function bounds
(tools/doa3-jp/functions.json.gz) but no bytes, so the mapping uses
function-size runs instead of Version Tracking:

- tools.disasm on the Europe XBE: 6,609 functions.
- Per section, align the Japan and Europe size sequences and keep runs of
  three or more equal sizes. 6,384 of 6,641 Japan functions (96.1 %) land in
  such runs.
- Library sections move as one block: D3D, D3DX, XGRPH, DSOUND, PSGSFD and
  XPP are all Japan + 0x940. .text is mostly + 0x970 (1,659 functions),
  with smaller blocks at + 0x8E0 and - 0x2.
- All 79 recipe addresses (manual_functions.json plus program_forwards_jp.c)
  map: 76 to function starts, 3 to interior points. 71 move + 0x940 and 8
  move + 0x970.
- Spot checks: D3DDevice_Swap (Japan 0x001E14D0) is Europe 0x001E1E10 and
  ends ret 4; IDirectSoundBuffer_SetHeadroom (Japan 0x001F1DA5) is Europe
  0x001F26E5 and ends ret 8, as on Japan.
- The XAPI TLS index is Europe 0x0040ABB8 (XBE header), Japan 0x0040A178:
  + 0xA40 for .data.

Data constants: of the 90 Japan guest addresses in DOA3(usa, japan), 63
shifted by + 0x940, + 0x970 or + 0xA40 hit an address the Europe code
references. 11 have no referenced candidate (task slots at 0x0073C400, the
0x009B7xxx block, 0x00C80078) and need reading in context; about 16 hit only
at an odd delta and are unconfirmed.

## Next

- A Europe recipe (tools/doa3-eur) and a DOA3 build selector with a third
  value, then the first lift and boot.
- The unexplained data constants and the 5 game seams on Europe code.
