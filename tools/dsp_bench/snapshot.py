"""Snapshots the MCPX GP DSP of a running xemu through its GDB stub.

Start xemu with -s and [audio] use_dsp = true, use_dsp_jit = true (the
interpreter mis-runs IFcc and collapses the reverb's taps). Connecting
pauses the VM at an APU frame boundary; each snapshot then holds the GP's P/X/Y
memory, mix buffer and scratch pages, and the I3DL2 listener DSOUND applied,
found by scanning guest RAM for the game's preset table entries.

    python tools/dsp_bench/snapshot.py private/dsp-bench/fight 10 12
    python tools/dsp_bench/snapshot.py private/dsp-bench/usa 10 12 --xbe private/imported-disc/disc/default.xbe

The preset table, USA 3.0 or Japan 3.1, is chosen from --xbe.
xemu accepts one GDB connection per launch.
"""
import argparse, json, socket, struct, time

APU, GP = 0xFE800000, 0xFE830000
LISTENER = struct.Struct('<iifffififfff')
# Listener presets: file offset, virtual address, entries. Both tables open
# with Microsoft's Generic preset; USA 3.0 holds only the standard list and a
# long Hallway (fights use Room, entry 2), Japan 3.1 adds 17 stage presets.
TABLES = {'USA 3.0': (0x323DB8, 0x3313F8, 25), 'Japan 3.1': (0x2C7E58, 0x2D3EB8, 41)}
GENERIC = LISTENER.pack(-1000, -100, 0.0, 1.49, 0.83, -2602, 0.007, 200, 0.011, 100.0, 100.0, 5000.0)


def preset_table(data):
    for name, table in TABLES.items():
        if data[table[0]:table[0] + LISTENER.size] == GENERIC:
            return name, table
    raise SystemExit('no known DOA3 listener preset table in this XBE')


class Gdb:
    def __init__(self, port):
        self.s = socket.create_connection(('127.0.0.1', port))
        self.s.settimeout(120)
        self.buf = b''

    def packet(self):
        while True:
            i = self.buf.find(b'$')
            j = self.buf.find(b'#', i + 1) if i >= 0 else -1
            if i >= 0 and j >= 0 and len(self.buf) >= j + 3:
                body, self.buf = self.buf[i + 1:j], self.buf[j + 3:]
                self.s.sendall(b'+')
                return body
            self.buf += self.s.recv(65536)

    def send(self, body):
        self.s.sendall(b'$' + body + b'#' + b'%02x' % (sum(body) & 0xff))

    def cmd(self, body):
        self.send(body)
        return self.packet()

    def read(self, addr, n):
        out = bytearray()
        while n:
            k = min(n, 0x800)
            r = self.cmd(b'm%x,%x' % (addr, k))
            if r.startswith(b'E'):
                raise IOError('read %08x: %s' % (addr, r))
            out += bytes.fromhex(r.decode())
            addr, n = addr + k, n - k
        return bytes(out)

    def physical(self, addr, n):
        if self.cmd(b'Qqemu.PhyMemMode:1') != b'OK':
            raise IOError('no physical memory mode')
        try:
            return self.read(addr, n)
        finally:
            self.cmd(b'Qqemu.PhyMemMode:0')

    def u32(self, addr):
        return struct.unpack('<I', self.read(addr, 4))[0]


def find_listener(g, entries, table_va, image_size):
    """Physical address of DSOUND's listener copy: an entry outside the XBE image."""
    ram = g.physical(0, 64 << 20)
    table = ram.find(b''.join(entries))
    if table < 0:
        return None
    base = table - (table_va - 0x10000)
    hits = set()
    for e in entries:
        at = ram.find(e)
        while at >= 0:
            if not base <= at < base + image_size:
                hits.add(at)
            at = ram.find(e, at + 1)
    return hits.pop() if len(hits) == 1 else None


def snapshot(g, path, listener_at):
    p, x = g.read(GP + 0xA000, 0x4000), g.read(GP + 0x0000, 0x4000)
    y, mix = g.read(GP + 0x6000, 0x2000), g.read(GP + 0x5000, 0x1000)
    sge, maxsge, gprst = g.u32(APU + 0x2040), g.u32(APU + 0x20D4), g.u32(GP + 0xFFFC)
    table = g.physical(sge, (maxsge + 1) * 8)
    pages = [struct.unpack_from('<I', table, i * 8)[0] for i in range(maxsge + 1)]
    scratch = b''.join(g.physical(a & 0xFFFFF000, 0x1000) for a in pages)
    listener = LISTENER.unpack(g.physical(listener_at, 48)) if listener_at is not None else None
    header = json.dumps({'gprst': gprst, 'pages': pages, 'listener': listener}).encode()
    with open(path, 'wb') as f:
        f.write(struct.pack('<I', len(header)) + header + p + x + y + mix + scratch)
    print('%s gprst=%x listener=%s' % (path, gprst, listener), flush=True)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('prefix')
    ap.add_argument('count', type=int)
    ap.add_argument('interval', type=float)
    ap.add_argument('--xbe', default='private/jp-disc/disc/Default.xbe')
    ap.add_argument('--port', type=int, default=1234)
    args = ap.parse_args()
    data = open(args.xbe, 'rb').read()
    name, (offset, table_va, count) = preset_table(data)
    print('%s presets at %08x' % (name, table_va), flush=True)
    entries = [data[offset + i * 48:offset + (i + 1) * 48] for i in range(count)]
    image_size = struct.unpack_from('<I', data, 0x10C)[0]
    g = Gdb(args.port)
    g.cmd(b'?')  # connecting already paused the VM
    listener_at = None
    for n in range(args.count):
        if n:
            g.s.sendall(b'\x03')
            g.packet()
        try:
            if listener_at is None:
                listener_at = find_listener(g, entries, table_va, image_size)
            snapshot(g, '%s%02d.gpsnap' % (args.prefix, n), listener_at)
        finally:
            g.send(b'c')
        time.sleep(args.interval)
    g.s.close()


if __name__ == '__main__':
    main()
