#!/usr/bin/env python3
"""Developer check: the Rust instruction lifter (wtbuild lift-words) writes exactly what lifter/recomp.py writes.

    python3 tools/wtbuild/compare_lifter.py [count] [--wtbuild <path>] [--seed N]

Needs no game files: recomp.py is loaded over a small stand-in ELF, and random instruction words (uniform, plus words
biased towards the FP / SIMD and load / store classes, plus every form test_ext.py and test_b3.py check) go through
Fn.lift of both. Calls are written as f_<target>(c) on both sides. Exits 1 on the first differences (up to 20 shown).
"""
import os
import random
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..', '..'))
BASE = 0x7100000000


def fake_elf():
    """an ELF with one PT_LOAD (text 16 MB) and a DYNAMIC whose tags recomp.py reads"""
    text = 1 << 24
    eh = b'\x7fELF\x02\x01\x01' + bytes(9) + struct.pack('<HHIQQQIHHHHHH', 3, 183, 1, 0, 0x40, 0, 0, 0x40, 0x38, 2, 0x40, 0, 0)
    dyn_off = 0x200
    ph = struct.pack('<IIQQQQQQ', 1, 5, 0x1000, 0, 0, text, text, 0x1000)
    ph += struct.pack('<IIQQQQQQ', 2, 6, dyn_off, 0, 0, 0x100, 0x100, 8)
    b = bytearray(eh + ph)
    b += bytes(dyn_off - len(b))
    for tag, val in ((7, 0), (8, 0), (23, 0), (2, 0), (6, 0), (5, 0), (0, 0)):
        b += struct.pack('<qQ', tag, val)
    b += bytes(0x1000 + text - len(b))
    return bytes(b)


def load_recomp():
    src = open(os.path.join(ROOT, 'lifter', 'recomp.py')).read()
    src = src.replace("IMG = open(ELF, 'rb').read()", 'IMG = _FAKE_IMG')
    g = {'__name__': 'recomp_cmp', '__file__': os.path.join(ROOT, 'lifter', 'recomp.py'), '_FAKE_IMG': fake_elf()}
    exec(compile(src, 'recomp.py', 'exec'), g)
    return g


def words(n, rnd):
    out = []
    for i in range(n):
        k = rnd.random()
        w = rnd.getrandbits(32)
        if k < 0.35:                                     # FP / SIMD: op0 x111
            w = (w & ~(7 << 25)) | (7 << 25)
        elif k < 0.55:                                   # loads and stores: op0 x1x0
            w = (w & ~(0xf << 25)) | (rnd.choice((4, 6, 12, 14)) << 25)
        elif k < 0.65:                                   # data processing, register
            w = (w & ~(0xf << 25)) | (rnd.choice((5, 13)) << 25)
        out.append(w)
    return out


def main():
    args = sys.argv[1:]
    exe = os.path.join(HERE, 'target', 'release', 'wtbuild')
    seed, count = 1, 400000
    while args:
        a = args.pop(0)
        if a == '--wtbuild':
            exe = args.pop(0)
        elif a == '--seed':
            seed = int(args.pop(0))
        else:
            count = int(a)
    R = load_recomp()

    class Env:
        def call(self, target):
            return 'f_%x(c);' % target

    rnd = random.Random(seed)
    ws = words(count, rnd)
    jobs, want = [], []
    for w in ws:
        pc = BASE + rnd.randrange(0, 1 << 22) * 4
        entry, end = pc - rnd.choice((0, 4, 64, 4096)), pc + rnd.choice((4, 8, 256, 1 << 16))
        fn = R['Fn'](entry, end, Env())
        try:
            s = fn.lift(w, pc)
        except R['Unimpl'] as e:
            s = 'UNIMPL %s' % e
        except Exception as e:                           # a decoder crash in recomp.py: not compared
            continue
        jobs.append('%x %x %x %x' % (w, pc, entry, end))
        want.append(('BR ' if fn.has_br else '') + s)
    r = subprocess.run([exe, 'lift-words'], input='\n'.join(jobs) + '\n', capture_output=True, text=True)
    if r.returncode:
        sys.exit('wtbuild lift-words failed:\n' + r.stderr[-3000:])
    got = r.stdout.splitlines()
    bad = [(j, a, b) for j, a, b in zip(jobs, want, got) if a != b]
    if len(got) != len(want):
        bad.append(('count', str(len(want)), str(len(got))))
    unimpl = sum(s.startswith('UNIMPL') for s in want)
    print('%d words compared (%d translate, %d not), %d differ' % (len(want), len(want) - unimpl, unimpl, len(bad)))
    for j, a, b in bad[:20]:
        print('  %s\n    recomp.py: %s\n    wtbuild:   %s' % (j, a, b))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
