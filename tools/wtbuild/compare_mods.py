#!/usr/bin/env python3
"""Developer check: `wtbuild mods-tree` / `mods-apply` say and write exactly what scripts/pchtxt.py tree / apply do, on
random mod folders (pchtxt blocks, IPS and IPS32 files, switches in a mods.ini, mistakes on purpose) made against a
stand-in main (testdata/fake_main: no game code).

    python3 tools/wtbuild/compare_mods.py [cases] [--seed N]

Needs clang and ld.lld with the AArch64 target and the Python lz4 package (for the stand-in).
"""
import os
import random
import shutil
import struct
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..', '..'))
WORDS = ['Speed', 'Infinite magic', 'No HUD', 'Camera', 'FPS', '', '', 'Fix', 'Fix', 'Unlock = all', 'Größe', '視点']


def pchtxt(r, bid, text_size):
    out = []
    if r.random() < .9:
        out.append('@nsobid-%s' % (bid if r.random() < .95 else ('%040x' % r.getrandbits(160))))
    if r.random() < .3:
        out.append('@flag offset_shift 0x100')
        shift = 0x100
    else:
        shift = 0
    for _ in range(r.randrange(1, 5)):
        if r.random() < .8:
            out.append('// ' + r.choice(WORDS))
        if r.random() < .2:
            out.append(r.choice(['@little-endian', '@big-endian', '@flag little-endian', '# a comment', '']))
        out.append(r.choice(['@enabled', '@enabled', '@disabled']))
        for _ in range(r.randrange(0, 5)):
            off = r.randrange(0, text_size - 64) + 0x100 - shift
            k = r.random()
            if k < .5:
                val = '%0*x' % (2 * r.randrange(1, 9), r.getrandbits(64))
            elif k < .75:
                val = '"%s"' % r.choice(['abc', 'a\\nb', '\\x41\\x42', 'q\\"x', '\\101\\7', '\\u00e9', 'tab\\there', 'é'])
            elif k < .77:
                val = '"%s"' % r.choice(['\\u4e2d', '\\x4', '\\q', '漢'])        # mistakes and odd escapes
            elif k < .78:
                val = 'abc'                                                       # odd number of digits
            elif k < .79:
                off = r.choice([text_size * 64, 0])                              # outside the program / before it
                val = 'aabb'
            else:
                val = '%08x' % r.getrandbits(32)
            out.append('%x %s%s' % (off, val, (r.choice(['', '', ' // note', '\t']) if r.random() < .98 else 'zz')))
    if r.random() < .1:
        out.append('@stop')
        out.append('garbage after stop')
    return '\n'.join(out) + r.choice(['\n', '', '\r\n'])


def ips(r, text_size, big):
    out = bytearray(b'IPS32' if big else b'PATCH')
    for _ in range(r.randrange(0, 6)):
        off = r.randrange(0, text_size - 64) + 0x100
        out += off.to_bytes(4 if big else 3, 'big')
        if r.random() < .3:
            out += struct.pack('>HHB', 0, r.randrange(1, 40), r.getrandbits(8))
        else:
            data = bytes(r.getrandbits(8) for _ in range(r.randrange(1, 12)))
            out += struct.pack('>H', len(data)) + data
    out += b'EEOF' if big else b'EOF'
    if r.random() < .2:
        out = out[:-2]                                  # truncated
    return bytes(out)


def make_case(r, d, bid, text_size):
    names = []
    for m in range(r.randrange(1, 5)):
        mod = r.choice(['Speed Mod', 'cam', 'Zeta', 'alpha', 'Ünicode mod', 'x'])
        for f in range(r.randrange(1, 4)):
            sub = r.choice(['', 'exefs/', 'exefs/', 'Extra/'])
            if r.random() < .3:
                name = r.choice(['%s.ips' % bid[:r.choice([40, 64, 32])], 'notabuildid.ips', bid[:40].upper() + '.IPS'])
                data = ips(r, text_size, r.random() < .5)
            else:
                name = '%s.pchtxt' % r.choice(['main', 'patch', 'Patch2', 'readme'])
                data = pchtxt(r, bid, text_size).encode('utf-8')
                if r.random() < .1:
                    data = b'\xef\xbb\xbf' + data
            p = os.path.join(d, mod, sub, name) if r.random() < .85 else os.path.join(d, name)
            os.makedirs(os.path.dirname(p), exist_ok=True)
            with open(p, 'wb') as f:
                f.write(data)
            names.append(os.path.relpath(p, d))
        if r.random() < .2:
            os.makedirs(os.path.join(d, mod, 'romfs'), exist_ok=True)
    if r.random() < .3:
        os.makedirs(os.path.join(d, 'Texture pack', r.choice(['romfs', 'inner/romfs'])), exist_ok=True)
    ini = ['[mods]']
    for n in names:
        if r.random() < .3:
            ini.append('%s = %s' % (r.choice([n, n.split(os.sep)[0], n.upper(), n + ' :: #1', n + '::' + r.choice(WORDS),
                                                n + '  ::  #2']), r.choice(['off', 'on', 'OFF ; why', '0', 'yes', 'maybe'])))
    ini.append(r.choice(['', '[other]\nx = off', '; comment']))
    return '\n'.join(ini) + '\n'


def run(cmd):
    p = subprocess.run(cmd, capture_output=True)
    return p.returncode, p.stdout, p.stderr


def main():
    args = sys.argv[1:]
    seed = 1
    if '--seed' in args:
        k = args.index('--seed')
        seed = int(args[k + 1])
        del args[k:k + 2]
    cases = int(args[0]) if args else 200
    exe = os.path.join(HERE, 'target', 'release', 'wtbuild')
    t = tempfile.mkdtemp(prefix='wtmods_')
    try:
        nso, elf = os.path.join(t, 'main'), os.path.join(t, 'main.elf')
        subprocess.run([sys.executable, os.path.join(HERE, 'testdata', 'fake_main', 'make_fake.py'), nso, '60'], check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        subprocess.run([sys.executable, os.path.join(ROOT, 'tools', 'extract', 'nso2elf.py'), nso, elf], check=True, stdout=subprocess.DEVNULL)
        bid = open(nso, 'rb').read()[0x40:0x60].hex()
        text_size, = struct.unpack_from('<I', open(nso, 'rb').read(), 0x18)
        r = random.Random(seed)
        bad = applied = 0
        py = [sys.executable, '-W', 'ignore', os.path.join(ROOT, 'scripts', 'pchtxt.py')]
        for i in range(cases):
            d = os.path.join(t, 'case%d' % i)
            os.makedirs(d)
            ini = os.path.join(t, 'mods%d.ini' % i)
            with open(ini, 'w', encoding='utf-8') as f:
                f.write(make_case(r, os.path.join(d, 'mods'), bid, text_size))
            mods = os.path.join(d, 'mods')
            for what, a, b in (
                    ('tree', py + ['tree', mods, nso, '--ini=' + ini], [exe, 'mods-tree', mods, nso, '--ini=' + ini]),
                    ('tree without main', py + ['tree', mods, '--ini=' + ini], [exe, 'mods-tree', mods, '--ini=' + ini]),
                    ('apply', py + ['apply', mods, nso, elf, os.path.join(d, 'py.elf'), '--ini=' + ini],
                     [exe, 'mods-apply', mods, nso, elf, os.path.join(d, 'rs.elf'), '--ini=' + ini])):
                ra, rb = run(a), run(b)
                if what == 'apply':
                    ra = (ra[0], ra[1].replace(b'py.elf', b'rs.elf'), ra[2])
                same = ra == rb
                if same and what == 'apply' and ra[0] == 0:
                    applied += 1
                    same = open(os.path.join(d, 'py.elf'), 'rb').read() == open(os.path.join(d, 'rs.elf'), 'rb').read()
                if not same:
                    bad += 1
                    if bad <= 3:
                        print('case %d %s differs:\n--- pchtxt.py (exit %d)\n%s%s\n--- wtbuild (exit %d)\n%s%s' % (
                            i, what, ra[0], ra[1].decode('utf-8', 'replace')[-1500:], ra[2].decode('utf-8', 'replace')[-800:],
                            rb[0], rb[1].decode('utf-8', 'replace')[-1500:], rb[2].decode('utf-8', 'replace')[-800:]))
        print('mods: %d cases (%d applied cleanly), %d differ' % (cases, applied, bad))
        return 1 if bad else 0
    finally:
        shutil.rmtree(t, ignore_errors=True)


if __name__ == '__main__':
    sys.exit(main())
