#!/usr/bin/env python3
"""Developer check: the code compiled on the player's PC comes out the same with runtime/freestanding (no libc headers,
as `wtbuild compile` and `wtbuild link` build it) as with the system's C library headers, byte for byte.

    python3 tools/wtbuild/compare_freestanding.py [--words N] [--clang <clang>] [--wtbuild <path>] [--target windows]
    python3 tools/wtbuild/compare_freestanding.py --gen build/gen_eh [--tables runtime/gen]    your own lifted code

Without --gen the lifted code is made up: N random instruction words (FP / SIMD and load / store heavy, as
compare_lifter.py) through `wtbuild lift-words`, one function each, so no game files are needed. --gen compiles your
build/gen_eh instead; --tables also compiles runtime/gen/sdkdata.c, sdklift_gen.c and runtime/wt_tables.c. A probe
checks that jmp_buf has glibc's size and alignment. Needs the system's libc headers (the side compared against).

--target windows compares against mingw-w64's headers instead (the UCRT ones the Windows runtime is built with), for
MinGW objects as `wtbuild` makes them on Windows: --clang names a clang that has them, such as llvm-mingw's.
"""
import os
import random
import re
import shutil
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..', '..'))
RT = os.path.join(ROOT, 'runtime')
FREE = ['-nostdlibinc', '-isystem', os.path.join(RT, 'freestanding')]
# compile.rs (the lifted code) and link.rs (the player's tables) flags; -g left out of the tables: debug info names the
# header files, which differ by design
LIFTED = ['-O2', '-ffp-contract=off', '-w', '-fno-strict-aliasing', '-mcx16', '-fPIC', '-Dinline=inline __attribute__((always_inline))']
TABLES = ['-O1', '-Wno-everything', '-fno-strict-aliasing', '-mcx16', '-fPIC', '-D__declspec(x)=']
BASE = 0x7100000000


def word_corpus(n, seed, exe, out):
    rnd = random.Random(seed)
    jobs = []
    for _ in range(n):
        k, w = rnd.random(), rnd.getrandbits(32)
        if k < 0.45:
            w = (w & ~(7 << 25)) | (7 << 25)
        elif k < 0.7:
            w = (w & ~(0xf << 25)) | (rnd.choice((4, 6, 12, 14)) << 25)
        pc = BASE + rnd.randrange(0, 1 << 22) * 4
        jobs.append('%x %x %x %x' % (w, pc, pc - rnd.choice((0, 64)), pc + rnd.choice((4, 256))))
    r = subprocess.run([exe, 'lift-words'], input='\n'.join(jobs) + '\n', capture_output=True, text=True)
    if r.returncode:
        sys.exit('wtbuild lift-words failed:\n' + r.stderr[-3000:])
    body = [s[3:] if s.startswith('BR ') else s for s in r.stdout.splitlines()]
    body = [s for s in body if not s.startswith('UNIMPL') and 'goto ' not in s]
    os.makedirs(out)
    names = []
    for i in range(0, len(body), 1500):
        part = body[i:i + 1500]
        calls = sorted(set(re.findall(r'\b(f_[0-9a-f]+)\(', ' '.join(part))))
        src = ['#include "runtime.h"', '#include "rt_ext.h"'] + ['void %s(Ctx *c);' % f for f in calls]
        src += ['void w_%d(Ctx *c) { %s }' % (i + j, s) for j, s in enumerate(part)]
        names.append(os.path.join(out, 'w_%04d.c' % (i // 1500)))
        with open(names[-1], 'w') as f:
            f.write('\n'.join(src) + '\n')
    print('%d instructions lifted into %d files' % (len(body), len(names)))
    return names


def main():
    args = sys.argv[1:]
    exe = os.path.join(HERE, 'target', 'release', 'wtbuild')
    clang, words, gen, tables, seed, windows = 'clang', 60000, None, None, 1, False
    while args:
        a = args.pop(0)
        if a == '--clang':
            clang = args.pop(0)
        elif a == '--wtbuild':
            exe = args.pop(0)
        elif a == '--words':
            words = int(args.pop(0))
        elif a == '--seed':
            seed = int(args.pop(0))
        elif a == '--gen':
            gen = os.path.abspath(args.pop(0))
        elif a == '--tables':
            tables = os.path.abspath(args.pop(0))
        elif a == '--target' and args and args[0] in ('linux', 'windows'):
            windows = args.pop(0) == 'windows'
        else:
            sys.exit(__doc__)
    lifted, tbl, target = LIFTED, TABLES, []
    if windows:                                          # target.rs: MinGW objects, no -fPIC (PE has none)
        lifted = [f for f in LIFTED if f != '-fPIC']
        tbl = [f for f in TABLES if f not in ('-fPIC', '-D__declspec(x)=')]
        target = ['--target=x86_64-w64-mingw32', '-mno-incremental-linker-compatible']   # no time stamp in the objects
    t = tempfile.mkdtemp(prefix='wtfree_')
    try:
        jobs = []                                        # (name, source, flags)
        if gen:
            for n in sorted(os.listdir(gen)):
                if n.endswith('.c'):
                    jobs.append((n, os.path.join(gen, n), lifted + ['-I', RT, '-I', gen]))
        else:
            gen = os.path.join(t, 'gen')
            for p in word_corpus(words, seed, exe, gen):
                jobs.append((os.path.basename(p), p, lifted + ['-I', RT, '-I', gen]))
        if tables:
            for p in (os.path.join(tables, 'sdkdata.c'), os.path.join(tables, 'sdklift_gen.c'), os.path.join(RT, 'wt_tables.c')):
                jobs.append((os.path.basename(p), p, tbl + ['-I', RT, '-I', tables]))
        probe = os.path.join(t, 'probe.c')
        with open(probe, 'w') as f:
            f.write('#include <setjmp.h>\n#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n#include <math.h>\n'
                    'const unsigned long jmp_size = sizeof(jmp_buf), jmp_align = _Alignof(jmp_buf);\n')
            if windows:                                  # setjmp as mingw-w64 expands it (the UCRT's _setjmp and a frame)
                f.write('int jump(jmp_buf b) { return setjmp(b); }\n'
                        'float mag(float x) { return fabsf(x); }\n')
            else:
                f.write('const unsigned long tag_mask = __builtin_offsetof(struct __jmp_buf_tag, __mask_was_saved);\n')
        jobs.append(('probe.c', probe, ['-O2']))

        def build(job):
            name, src, flags = job
            res = []
            for side, extra in (('sys', []), ('free', FREE)):
                o = os.path.join(t, side, name + '.o')
                os.makedirs(os.path.dirname(o), exist_ok=True)
                p = subprocess.run([clang] + target + ['-c'] + flags + extra + [src, '-o', o], capture_output=True, text=True)
                if p.returncode:
                    return name, '%s headers: %s' % (side, p.stderr[-1500:])
                res.append(open(o, 'rb').read())
            return name, None if res[0] == res[1] else 'objects differ'

        with ThreadPoolExecutor(os.cpu_count() or 4) as ex:
            results = list(ex.map(build, jobs))
        bad = [(n, why) for n, why in results if why]
        for n, why in bad[:10]:
            print('  %s: %s' % (n, why))
        print('freestanding: %d files compiled both ways, %d differ' % (len(results), len(bad)))
        return 1 if bad else 0
    finally:
        shutil.rmtree(t, ignore_errors=True)


if __name__ == '__main__':
    sys.exit(main())
