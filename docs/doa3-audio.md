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
through the normal game path. Listener/voice 3D state, effects images and mix-bin routing still
succeed without modeling their DSP. StopEx supports immediate stop and loop
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
  left/right pairs of 31 int8 taps plus a delay byte) and index map
  `0x001E0448` ([61][31] u16). `dsound_hrtf.c` resamples the voice linearly to
  48 kHz, applies the two FIRs with the interaural delay and crossfades when the
  filter changes. DSOUND uploads the table bytes to the APU unchanged
  (`0x001CA481`), so their scale is a hardware property the XBE does not hold.
  The renderer follows xemu (`hw/xbox/mcpx/apu/vp/hrtf.h`, commit `478b4f4`):
  each ear's taps are divided by their absolute sum, so no ear exceeds its
  input peak. This is not hardware-calibrated, and it narrows interaural level
  differences (about 0.8 dB instead of 9.4 dB at 30 degrees). 3D voices reach
  XAudio2 as float, so volume and distance apply before any saturation.

An earlier global scale (`1 / (128 * 2.71)`) boosted the near ear by up to
12.7 dB at low frequencies and saturated 2-4% of 3D voice samples in fights,
heard as blown-out hits. With the current renderer two paced fights measure a
mix peak of 0.29 with no samples near full scale, against 0.88-1.00 before;
fight RMS is -28 dB, about 5.5 dB below the earlier stereo-pan build.

Rear sources are not folded to the front, and I3DL2 reverb and the front/rear
speaker split are not modeled. `RECOMP_AUDIO_TRACE=1` logs the
positioning and mix-bin calls.
Owned CreateSoundBuffer storage uses the reusable kernel pool and is reclaimed
on Release; caller-supplied sample memory remains caller-owned.

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
