"""Compares the GP DSP's I3DL2 reverb with the runtime's for one snapshot.

Feeds the same click into the I3DL2 mix bin (10) of the snapshot and into
dsound_reverb.c with the snapshot's listener, then reports both responses.

    python tools/dsp_bench/compare.py private/dsp-bench/fight00.gpsnap
"""
import json, os, struct, subprocess, sys
import numpy as np
from scipy.signal import butter, sosfiltfilt

RATE, AMP, I3DL2_BIN = 48000, 0.5, 10
BENCH = os.path.join('private', 'dsp-bench')


def xemu_response(snap, out):
    subprocess.run([os.path.join(BENCH, 'dspbench.exe'), 'impulse', snap, str(I3DL2_BIN), '200', '3000', str(AMP), out], check=True)
    bins = np.fromfile(out, np.float32).reshape(-1, 32, 32).transpose(1, 0, 2).reshape(32, -1)
    return bins[0], bins[1]  # xemu plays bins 0 and 1 as the stereo output


def runtime_response(listener, out):
    subprocess.run([os.path.join(BENCH, 'reverb_ir.exe'), *map(str, listener), str(AMP), out], check=True)
    pcm = np.fromfile(out, np.float32).reshape(-1, 2)
    return pcm[:, 0], pcm[:, 1]


def report(name, left, right, late_ms):
    e = left ** 2 + right ** 2
    db = lambda v: 10 * np.log10(v / 2 / AMP ** 2 + 1e-30)
    edc = np.cumsum(e[::-1])[::-1]
    edc = 10 * np.log10(edc / edc[0] + 1e-30)
    at = lambda level: np.argmax(edc < level) / RATE * 1000 if (edc < level).any() else float('nan')
    split = int(late_ms * RATE / 1000)
    bands = []
    for fc in (250, 1000, 4000):
        sos = butter(4, [fc / 1.414, fc * 1.414], btype='band', fs=RATE, output='sos')
        b = sosfiltfilt(sos, left) ** 2 + sosfiltfilt(sos, right) ** 2
        d = np.cumsum(b[::-1])[::-1]
        d = 10 * np.log10(d / d[0] + 1e-30)
        bands.append('%d Hz %.2f s' % (fc, (np.argmax(d < -35) - np.argmax(d < -5)) / RATE * 2))
    onset = np.argmax(np.abs(left) + np.abs(right) > 1e-7) / RATE * 1000
    print('%-8s onset %5.1f ms | total %6.1f dB | early %6.1f dB | late %6.1f dB | peak %.4f | -60 dB at %4.0f ms | T30 %s'
          % (name, onset, db(e.sum()), db(e[:split].sum()), db(e[split:].sum()),
             np.abs(np.concatenate([left, right])).max() / AMP, at(-60), ', '.join(bands)))


def main(snap):
    with open(snap, 'rb') as f:
        listener = json.loads(f.read(struct.unpack('<I', f.read(4))[0]))['listener']
    if listener is None:
        sys.exit('%s has no listener; take it with snapshot.py' % snap)
    stem = os.path.splitext(snap)[0]
    print('listener', listener)
    late_ms = (listener[6] + listener[8]) * 1000
    report('xemu', *xemu_response(snap, stem + '.xemu.f32'), late_ms)
    report('runtime', *runtime_response(listener, stem + '.runtime.f32'), late_ms)


if __name__ == '__main__':
    main(sys.argv[1])
