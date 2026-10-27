#!/usr/bin/env python3
"""Developer check: wtbuild's NSO conversion and whole-program lift write exactly what tools/extract/nso2elf.py and
lifter/recomp_eh.py write, on a stand-in program (testdata/fake_main: no game code) or on your own main.

    python3 tools/wtbuild/compare_lift.py [--functions N] [--seed N]       a stand-in main built with clang for AArch64
    python3 tools/wtbuild/compare_lift.py --main <main NSO>                  your dump's main (stays on your PC)

The stand-in is converted and lifted by both without the Options-row instruction patches (their addresses are main's);
your main is lifted with them. Every output file is compared byte for byte. Needs lz4 for nso2elf.py; the stand-in also
needs clang and ld.lld with the AArch64 target.
"""
import filecmp
import os
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..', '..'))


def run_recomp_eh(elf, outdir, patches, tmp):
    """recomp_eh.py's main on `elf`: copies of recomp.py / regcache.py / recomp_eh.py that read `elf` instead of
    ../targets/exefs/main.elf (and, for the stand-in, skip the Options-row patches)."""
    work = os.path.join(tmp, 'lifter')
    os.makedirs(work, exist_ok=True)
    for n in ('recomp.py', 'regcache.py', 'recomp_eh.py'):
        src = open(os.path.join(ROOT, 'lifter', n)).read()
        if n == 'recomp.py':
            src = src.replace("IMG = open(ELF, 'rb').read()", 'IMG = open(%r, \'rb\').read()' % elf)
        if n == 'recomp_eh.py' and not patches:
            src = src.replace('\n_options_row_patches()', '\npass')
        with open(os.path.join(work, n), 'w') as f:
            f.write(src)
    env = {k: v for k, v in os.environ.items() if not k.startswith(('RECOMP_', 'RECOMPS_'))}
    subprocess.run([sys.executable, 'recomp_eh.py', outdir, '4'], cwd=work, check=True, env=env)


def same_tree(a, b):
    na, nb = sorted(os.listdir(a)), sorted(os.listdir(b))
    if na != nb:
        return ['file lists differ: only in recomp_eh: %s; only in wtbuild: %s' % (sorted(set(na) - set(nb))[:5], sorted(set(nb) - set(na))[:5])]
    return [n for n in na if not filecmp.cmp(os.path.join(a, n), os.path.join(b, n), shallow=False)]


def main():
    args = sys.argv[1:]
    exe = os.path.join(HERE, 'target', 'release', 'wtbuild')
    nso, nfuncs, seed = None, 400, 1
    while args:
        a = args.pop(0)
        if a == '--main':
            nso = os.path.abspath(args.pop(0))
        elif a == '--functions':
            nfuncs = int(args.pop(0))
        elif a == '--seed':
            seed = int(args.pop(0))
        elif a == '--wtbuild':
            exe = os.path.abspath(args.pop(0))
    t = tempfile.mkdtemp(prefix='wtcmp_')
    try:
        patches = nso is not None
        if nso is None:
            nso = os.path.join(t, 'main')
            subprocess.run([sys.executable, os.path.join(HERE, 'testdata', 'fake_main', 'make_fake.py'), nso, str(nfuncs),
                            '--seed', str(seed)], check=True)
        py_elf, rs_elf = os.path.join(t, 'py.elf'), os.path.join(t, 'rs.elf')
        a = subprocess.run([sys.executable, os.path.join(ROOT, 'tools', 'extract', 'nso2elf.py'), nso, py_elf], capture_output=True, text=True)
        b = subprocess.run([exe, 'nso2elf', nso, rs_elf], capture_output=True, text=True)
        if a.returncode or b.returncode:
            sys.exit('conversion failed:\n%s%s%s' % (a.stdout, a.stderr, b.stderr))
        ok = True
        if a.stdout.splitlines()[:-1] != b.stdout.splitlines()[:-1]:          # all but "wrote <path>"
            ok = False
            print('nso2elf output differs:\n%s---\n%s' % (a.stdout, b.stdout))
        if open(py_elf, 'rb').read() != open(rs_elf, 'rb').read():
            ok = False
            print('the converted ELFs differ')
        else:
            print('nso2elf: the same ELF (%d bytes)' % os.path.getsize(py_elf))
        py_out, rs_out = os.path.join(t, 'py_gen'), os.path.join(t, 'rs_gen')
        run_recomp_eh(py_elf, py_out, patches, t)
        r = subprocess.run([exe, 'lift', py_elf, rs_out] + ([] if patches else ['--no-patches']), capture_output=True, text=True)
        print(r.stdout, end='')
        if r.returncode:
            sys.exit('wtbuild lift failed:\n' + r.stderr)
        diff = same_tree(py_out, rs_out)
        print('lift: %d files, %d differ%s' % (len(os.listdir(py_out)), len(diff), (': ' + ', '.join(diff[:10])) if diff else ''))
        for n in diff[:3]:
            if n.endswith(('.c', '.h')):
                subprocess.run(['diff', os.path.join(py_out, n), os.path.join(rs_out, n)])
        ok &= not diff
        return 0 if ok else 1
    finally:
        shutil.rmtree(t, ignore_errors=True)


if __name__ == '__main__':
    sys.exit(main())
