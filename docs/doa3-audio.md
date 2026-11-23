# DOA3 audio

`dsound_api_adapter.c` replaces the DSOUND 3936 public buffer APIs. It submits
PCM to the existing XAudio2 backend, using the copied Xbox ADPCM decoder for
format `0x69`. CRI still decodes ADX, fills its PCM rings and controls playback.
No APU emulation or game-owned sound logic is introduced.

## Playback and timing

The adapter reads each buffer's WAVEFORMATEX instead of treating its average
byte rate as a sample rate. PCM supports mono/stereo, unsigned 8-bit and signed
16-bit samples. Xbox ADPCM uses 36-byte blocks and 64 decoded samples per
channel; the final nibble is padding (zero in all 3,477 mono and 568 stereo
channel blocks DOA3 created in a fight). Guest positions remain compressed
byte offsets for ADPCM.

Play, Stop, StopEx, SetBufferData, Lock, GetCurrentPosition, SetFrequency and
SetVolume share the existing buffer model. Loop regions now retain their end.
The host producer has a separate consumption cursor on the same monotonic
clock as guest polls. It copies guest samples before the decoder refills them;
a locked producer services buffers while the game thread is busy. Host queue
latency never drives guest progress.

GetCurrentPosition reports the producer's cursor, which trails the clock by up
to 15 ms. Reporting the clock let the game refill ring data before the
producer copied it: the music ring then spliced in the next lap at 30% of
submissions, heard as static. With the trailing cursor a paced fight had no
such splices and zero underruns. SetFrequency retimes the producer without
advancing it, so pitch changes between sends keep the unsent audio.

Netplay, replays and SyncTest run guest time on the frame-locked clock, which
falls behind real time when frames run slow and jumps when the game waits, so
both runners' cursors agree. Host output there keeps its own place in each
buffer on the real clock, outside the rollback state: a one-shot plays on to
its end, a ring runs up to 50 ms ahead into its loop region, and output falls
up to 100 ms behind the guest's cursor before it skips to it. Both cursors the
game reads stay on the game thread and guest time. A re-simulated frame
neither plays nor moves that place, a rewind neither replays nor loses what
was heard, and a buffer's created identity and its restarted generations
resync it. The output pump thread runs on this clock too and feeds only that
place. Rings still gap when slow frames outrun the game's refills.

The watchdog exits on another thread. Audio shutdown joins the producer and
parks further guest audio calls under the adapter lock before destroying
XAudio2 voices. This prevents submission during backend teardown.

The host master gain defaults to 1. `RECOMP_AUDIO_GAIN=0` mutes output;
values between 0 and 1 reduce it. Guest buffer volume remains in hundredths
of a decibel.

DSOUND 3936 voice initialization (`0x001C76DF`) reserves 600 hundredths
of dB for ordinary buffers, and zero for CTRL3D/mix-in buffers (flags
`0x82010`). SetVolume (`0x001C6E45`) subtracts that reserve. The public
adapter now applies both rules; previously it passed the requested volume
straight to XAudio2. The DOAXBV path already reads the library's adjusted
voice volume. This is voice headroom, separate from SetMixBinHeadroom's
integer shifts; neither voice count nor a host limiter changes the gain.
The synthetic check covers default/updated 2D volume and unchanged 3D gain.

A natural fight capture with the correction peaks at 0.404 on the front
pair, with no dropped chunks, underruns or engine glitches. The preceding
baseline peaks at 0.407 and reaches a different opponent/stage, so those
runs do not establish reproduction and elimination of the reported 1.23
combat peak. Neither run calls SetMixBins or SetMixBinVolumes.
Further corrected fight captures peak at 0.620, 0.345 and 0.496, with zero
dropped chunks, underruns or engine glitches. Master gain remains one.
The paced recheck after merging the shared clock and buffer recycling peaks
at 0.653, again with zero dropped chunks, underruns or engine glitches.

## Additional public bindings

The call audit found these entries beyond the original 26 wrappers:

| API | DOA3 entry | Evidence |
|---|---|---|
| SetCooperativeLevel | `0x001C6B72` | Retail leaf returns success and pops three arguments |
| Buffer Unlock | `0x001C6B77` | Retail leaf returns success and pops five arguments |
| DirectSoundUseFullHRTF | `0x001C6B92` | DSOUND 3911 signature and selector target match in the 3936 image |

All three are manual entries in the `audio` block. The HRTF selector succeeds; the
adapter reads the HRTF tables from the image directly. No other direct call from outside
DSOUND into its section remains generated in the audited snapshot.

## Capture and verification

Set `RECOMP_AUDIO_CAPTURE` to a new WAV filename under `private/` for an
opt-in recording of the XAudio2 mastering voice's mixed float samples.
The capture includes silence and the device's channel count. It is an
in-place pass-through [XAPO](https://learn.microsoft.com/en-us/windows/win32/xaudio2/how-to--create-an-xapo).
The audio callback only copies into preallocated memory; shutdown writes
the WAV. Existing files are never overwritten. Capture retains the first
60 seconds; a forced process kill cannot finalize it.

`RECOMP_AUDIO_TRACE=1` logs format, data, frequency, Play and Stop calls.
The backend's summary reports submitted/nonzero chunks, dropped chunks,
underruns and engine glitches. Keep recordings, measurements and frames
under `private/`.

The adapter check uses synthetic PCM and ADPCM to check ring wrapping,
finite loops, seeks, guest cursors and argument cleanup. The capture check
verifies pass-through samples, silence and capacity without opening a device.

Intro sound, title ambience, menu music and selection SFX have been captured
through the normal game path. Effects images and mix-bin routing still
succeed without modeling their DSP; the I3DL2 listener reverb is rendered
natively (below). StopEx supports immediate stop and loop
release; nonzero timestamps and release envelopes remain unimplemented.

Fight SFX are mono 22,050 Hz ADPCM voices created with CTRL3D (flags
0x40010). DOA3 keeps the listener at the origin with zero velocity, sets each
voice's listener-space position every frame (about 14,700 calls in a fight),
sets small voice velocities and calls SetMinDistance(10). The adapter models
the DSOUND 3936 per-voice terms DOA3 uses, with the listener factors, cones and
orientation left at their defaults:

- Distance (`0x001C9F93`, rolloff 1): `trunc(-2000 * log10(d / min))` hundredths
  of dB, with d clamped to [min, max] and the result to [-10000, 0]. Defaults are
  min 1 and max 1e9. It adds to the SetVolume level.
- Doppler (`0x001CA31A`): `min(10, 1 - u / 342)`, u being the relative velocity
  along the unit direction (positive when receding), clamped to +/-341. The
  pitch multiplies the SetFrequency rate and clamps to 187.53-191967.5 Hz
  (`0x001C9B9A`).
- HRTF: the direction becomes azimuth/elevation by the library's ratio
  approximation (`0x001C9DA1`), quantized and ear-swapped as in `0x001CB509`.
  The filter pair comes from the game's own tables: bank `0x001CEE88` (1,111
  left/right pairs of 31 taps plus a delay byte) and index map `0x001E0448`
  ([61][31] u16). `dsound_hrtf.c` resamples the voice linearly to 48 kHz,
  applies the two FIRs with the interaural delay and crossfades when the
  filter changes. 3D voices reach XAudio2 as float, so volume and distance
  apply before any saturation.
- Tap format: DSOUND uploads the table bytes unchanged (`0x001CA481`;
  `0x001CA3E6` interleaves left and right per PIO write). The APU reads each
  tap as sign, 3-bit exponent and 4-bit mantissa, from 1/2048 to 31/32, and
  sums the taps without normalization. Source: hardware readback by test
  homebrew in xemu PR #3105 (open, unreviewed): 0x40 is 1/16, 0x20 1/64,
  0x7F 31/32, 0x81 -1/2048. The game's table agrees independently: no tap
  byte is 0xF0-0xFF, which two's complement would need for small negatives.
  Decoded so, the near ear is 6-7 dB over the far ear at 30-90 degrees; a
  frontal voice passes about -9 dB in the speech band. No filter amplifies:
  DC gain is 0.28-0.61 per ear and the peak response about 0.67 (-3.5 dB),
  the HRTF attenuation of about 6 dB that the XDK documents. The absolute tap
  sum, the bound on any sample peak, is at most 1.42. The port previously
  read the taps as two's complement and divided each ear by its absolute sum
  (xemu master), which turned small negatives into spikes and inverted the
  interaural level difference.
- Mix routing follows the hardware. xemu's voice table (read through its GDB
  stub in a USA session) shows every 3D voice with volume on HRTF slots 0 and
  2 (left ear to bin 6, right ear to bin 7, from `0x09070806` in
  `0x001CCE10`) and the same volume on slot 4, bin 10, the I3DL2 send; rear
  bins 8 and 9 are muted. DSOUND init (`0x001C90C0`) sets mix-bin headroom 1,
  which halves every bin but 31, the HRTF bins included (PR #3105 measures
  HRTF headroom as having no effect), and the GP program plays bins 0/1 and
  6/7 at x2. So 2D and HRTF voices come out at their volume, and the I3DL2
  send, which carries the voice before the HRTF (PR #3105), reaches the
  reverb at half. Before, the send took the full volume against
  normalized, quieter 3D voices, and yells echoed (#108).

Rear sources are not folded to the front, and the front/rear speaker split
is not modeled. `RECOMP_AUDIO_TRACE=1` logs the positioning, mix-bin and
I3DL2 listener calls.
Owned CreateSoundBuffer storage uses the reusable kernel pool and is reclaimed
on Release; caller-supplied sample memory remains caller-owned.

## I3DL2 reverb

USA 3.0, from the code: at sound init (`0x0009F730`) the game downloads
`d:\dsstdfx.bin`, the standard effects image, and sets the I3DL2 listener
to entry 2 of a 48-byte DSI3DL2LISTENER table at `0x003313F8`. The table is
Microsoft's preset list starting at Generic, so entry 2 is Room; nothing
writes the table, and the current entry (`0x006A7910`) has no other writer.
While the game speed (`0x002FD558`, `0x0030A420`) is below 1.0, five call
sites switch to entry 12, Hallway (`0x0009F710`), and restore the current
entry when it returns (`0x0009F6F0`): the KO slowdown gets a longer tail.

Japan 3.1 reaches the same handler through its forward of `0x001F2A27`.
Its table (`0x002D3EB8`) holds the same 24 entries, the last one off (room
-10000), followed by 17 per-stage entries close to Cave with a short decay:
room -1000, decay 0.14-0.82 s, HF ratio 1.3, reflections about -603 at
15 ms, reverb -159 to -2318 at 22-42 ms. An attract run
(`RECOMP_AUDIO_TRACE=1`) starts with the listener off, then sets entries
from this range per stage.

The game never calls SetI3DL2Source, and its only SetMixBins calls are
CRI's speaker masks, so only CTRL3D voices feed the reverb, through their
default I3DL2 mix bin. DSOUND's mix-bin volumes (`0x001C99A4`) give that
send (slot 4) the voice volume, distance and cone terms the direct bins
get, plus the I3DL2 room term from `0x001CBAEE`: the source's lRoom and
occlusion, less listener and source room rolloff times 2000 log10(distance).
DOA3 never sets a DSI3DL2BUFFER, so that term is 0, and the send fades with
distance exactly as the dry sound does. Each 3D voice adds its 48 kHz mono
signal at that level to a shared send bus. The bus renders 10 ms behind the
clock, about as far as a voice's pump trails it; a later send is delayed to
the next unrendered frame and stays contiguous. The return plays through
host slot 256 (`dsound_reverb.c`) in pieces of at least 10 ms: 5 ms pieces
filled XAudio2's 16-buffer queue and dropped wet audio, heard as crunch on
hits. The effects image's DSP program is not run: the
reverb is a four-line feedback delay network driven by the preset (Room and
RoomHF input filter, reflections taps after ReflectionsDelay, two diffusion
allpasses, decay time and HF ratio per line, late energy at Room + Reverb).
Its sound is an approximation, not a hardware match; density and room
rolloff are ignored, and dry and wet play through separately cushioned host
voices, so they align only to a few milliseconds.

`tools/dsp_bench` measures that gap against the effects image itself.
`build.ps1` fetches xemu's DSP interpreter at v0.8.139 into
`private/dsp-bench` and builds `dspbench.exe` and `reverb_ir.exe`.
`snapshot.py` saves the GP DSP's memory, scratch pages and current listener
from xemu (started with `-s` and `[audio] use_dsp = true`; without it xemu
plays dry). `compare.py <snap>` feeds one click into the I3DL2 mix bin (10)
and into `dsound_reverb.c` and prints level, early, late and tail energy
(the tail starts 50 ms after the late onset), peak and T30 per band for
both. GP output is mix bins 0 and 1. The effect runs at 24 kHz (each output
sample is held twice). Snapshot with `[audio] use_dsp_jit = true`: xemu's
interpreter runs the conditional `clr a` (IFcc) at `p:02AE`
unconditionally, so every preset change after boot collapses the reflection
and late taps (X `0x1A3`..`0x1F7`) onto the write pointer. The reverb's
per-sample code has no other IFcc, so the bench runs a JIT snapshot
faithfully.

Its reflections are a tap pair per ear repeating every 6-7 ms at x0.4, each
tap about 11 dB under Room + Reflections. The late part starts at
ReflectionsDelay + ReverbDelay with a +/- tap pair per ear (left at 0 and
2.5 ms, right at 5.7 and 9.7 ms). The pair repeats once 60/70 ms later at
0.8x the decay over that time, then the diffuse tail follows. For Room most
of the late energy is in the pair, which fuses with the direct sound; the
tail is 10 dB under it. DSOUND (`0x001CBA59`) sets each line to -60 dB per
DecayTime, and the tail decays at that rate. `dsound_reverb.c` copies the
pair and repeat, starts its network 50 ms after the onset, and scales the
tail by 0.85 (DecayTime / 1.49)^1.43 (fitted to two presets, held at 1.49 s
for longer decays). Measured with
the JIT, against the runtime:

| Preset | Total | Tail | T30 at 1 kHz |
|---|---|---|---|
| USA Room (fights) | -10.8 / -10.9 dB | -21.5 / -22.4 dB | 0.40 / 0.40 s |
| USA Hallway (KO slowdown) | -3.5 / -4.0 dB | -7.4 / -8.0 dB | 1.49 / 1.46 s |
| Japan entry 30 | -20.2 / -20.0 dB | -36.4 / -49.3 dB | 0.65 / 0.23 s |

Japan 3.1's DSOUND rings its 0.2 s preset for about 1 s, so the runtime's
Japan tails are short and quiet; they sit 25 dB or more under the
reflections. The previous network had no pair and started at 35 ms with its
tail 6.5 dB over the effect's on Room, heard as slapback on yells (#108).

## xemu and hardware

xemu is the usual reference for how DOA3 sounds, but its master branch
(through v0.8.139) differs from the hardware in the 3D path. Hardware here
means the readback in xemu PR #3105 (one author, open and unreviewed) and the
XDK audio guide; no recording of a console's HRTF or reverb output was found.

| Behavior | Hardware | xemu master | Port |
|---|---|---|---|
| HRIR tap format | sign, 3-bit exponent, 4-bit mantissa (PR #3105) | two's complement | as hardware |
| Tap scaling | as written; per-ear DC 0.28-0.61, the documented 6 dB HRTF attenuation | each ear divided to unity gain | as hardware |
| Headroom on HRTF routes | mix-bin headroom; HRTF headroom has no effect (PR #3105) | HRTF headroom only (0 in DOA3) | as hardware |
| I3DL2 send | the voice before the HRTF (PR #3105, DSOUND's routing) | the HRTF left ear (filtered in place) | as hardware |
| Filter change | moves to the target across one frame (PR #3105, MCP FAQ) | one-pole smoothing, alpha 0.01 | crossfade across one pump |
| GP reverb program | runs `dsstdfx.bin` | interpreter runs IFcc at `p:02AE` unconditionally; JIT correct | fitted to the JIT (see above) |

So under master, 3D voices are louder and less directional than on a
console, and the reverb is fed a filtered, one-eared copy, so it sounds
drier. With `use_dsp` off, xemu plays no reverb at all. PR #3105's testers
report the boot sound and Halo's echoes matching a console. Compare the port
against xemu with that PR, the DSP on and the JIT on.

## Movie clock

Sofdec selects its vblank clock callback (`0x0017E2F0`), which reports the
refresh accumulator rather than the DirectSound play cursor. The cooperative
ADXM vblank workers previously serviced one tick per wake, losing elapsed
refreshes whenever decoding or rendering delayed them. Native replacements
for worker entries `0x0016A570` and `0x0016A5E0` now service the elapsed 60 Hz
refreshes before resuming the decoder. The registered middleware servers,
pause/stop state and frame-drop decisions remain generated; that temporary
boundary is marked `av` beside the manual bindings.

Source-frame matching over 12.3 seconds puts the corrected picture within
47 ms of the playback clock without accumulating delay. The preceding run
was 1.1 seconds behind by 3.9 seconds. Audio correlation at source seconds
1 through 11 measures a steady 70-74 ms output delay, including the backend's
50 ms cushion. Picture therefore leads audible output by a small, bounded
amount; exact display/device latency compensation remains unimplemented.
These are timestamp/capture measurements, not a perceptual sync claim.
After integrating the shared clock and buffer recycling, a paced recheck
shows a temporary picture delay of 212 ms during decoding, recovering to
within 11 ms of the source timeline at 13.6 seconds. Measured audio delay is
steady at 40-44 ms. The earlier continuously accumulating clock drift is absent.

Packed decode work now uses native SSE2 for the runtime's MMX operations.
Byte, word and 64-bit RAM accesses use the same checked inline path as
32-bit loads; packed loads and stores avoid the generic copy-call boundary.
These changes reduce decoder execution cost without changing the CRI clock,
cooperative scheduling or unpaced wait semantics. Watches, access logging,
pending device writes and RAM boundaries retain their checked fallback.
Exact output-latency compensation remains unimplemented.
The worker uses the shared guest performance clock, including skipped waits
in `RECOMP_UNPACED=1`. Playback synchronization and sample levels must be
measured in paced runs; accelerated runs can overflow the host audio queue.
