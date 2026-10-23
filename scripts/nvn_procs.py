"""[b3recomp] Every NVN entry point of YOUR sdk (Bayonetta 3's nnSdk 13.3.2) by name, read from the sdk's own proc table.

    python scripts/nvn_procs.py [targets/exefs/sdk.elf] [name ...]     prints "addr <hex> <name>" (sdklift_roots.txt lines)

NVN 13.3.2 keeps no "nvn..." strings: nvnDeviceGetProcAddress binary-searches a sorted blob of names without the "nvn"
prefix (u32 offsets into it, one per entry) and takes the function from a parallel array of pointers in data (relocated
R_AARCH64_RELATIVE). The search function is found by its shape: adrp+add of the names base and of the offset table,
"mov wN, #count - 1", and the pointer array's GOT slot. Nothing is hard-coded; docs/RE_NOTES.md "NVN" has the details.
"""
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', 'lifter'))
from recomp import _layout                              # noqa: E402  (reads the ELF given below, not main.elf)


def sx(v, b):
    return v - (1 << b) if v & (1 << (b - 1)) else v


def procs(path):
    d = open(path, 'rb').read()
    text, dt = _layout(d)
    rel = {}
    for i in range(dt[8] // 24):
        off, info, add = struct.unpack_from('<QQq', d, 0x1000 + dt[7] + i * 24)
        if info == 0x403:
            rel[off] = add
    key = d.find(b'\0DeviceGetInteger\0') + 1 - 0x1000
    ws = struct.unpack_from('<%dI' % (text // 4), d, 0x1000)

    def refs(lo, hi):
        """adrp+add (and adrp+ldr GOT) targets of the code in [lo, hi): [(kind, target)]"""
        page, out = {}, []
        for k in range(max(lo, 0), min(hi, len(ws))):
            w = ws[k]
            if w & 0x9f000000 == 0x90000000:
                page[w & 31] = ((k * 4) & ~0xfff) + (sx((((w >> 5) & 0x7ffff) << 2) | ((w >> 29) & 3), 21) << 12)
            elif w & 0xffc00000 == 0x91000000 and (w >> 5) & 31 in page:
                out.append(('add', page[(w >> 5) & 31] + ((w >> 10) & 0xfff)))
            elif w & 0xffc00000 == 0xf9400000 and (w >> 5) & 31 in page:
                out.append(('got', page[(w >> 5) & 31] + ((w >> 10) & 0xfff) * 8))
            elif w & 0xffe0001f == 0x52800017:
                out.append(('count', ((w >> 5) & 0xffff) + 1))   # mov w23, #count - 1 (the search's upper bound)
        return out

    for k, (kind, names) in [(k, r) for k in range(len(ws)) for r in refs(k - 1, k + 1)[-1:]
                             if r[0] == 'add' and r[1] <= key < r[1] + 0x10000]:
        around = refs(k - 24, k + 40)
        counts = [v for kd, v in around if kd == 'count']
        gots = [v for kd, v in around if kd == 'got' and v in rel]
        for table in [v for kd, v in around if kd == 'add' and v != names]:
            for count in counts:
                for got in gots:
                    try:
                        out = {}
                        for n in range(count):
                            no = struct.unpack_from('<I', d, 0x1000 + table + 4 * n)[0]
                            q = 0x1000 + names + no
                            out['nvn' + d[q:d.index(b'\0', q)].decode('latin-1')] = rel.get(rel[got] + 8 * n)
                    except (struct.error, ValueError):
                        continue
                    if 'nvnDeviceGetInteger' in out and list(out) == sorted(out) and all(out.values()):
                        return out
    raise SystemExit('%s: no NVN proc table found (not nnSdk 13.3.2?)' % path)


if __name__ == '__main__':
    args = sys.argv[1:]
    path = args.pop(0) if args and args[0].endswith('.elf') else os.path.join(HERE, '..', 'targets', 'exefs', 'sdk.elf')
    table = procs(path)
    for name in args or table:
        print('addr %x %s' % (table[name], name) if table.get(name) else '# %s: not in this sdk' % name)
