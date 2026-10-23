"""[b3recomp] Every NVN entry point of YOUR sdk (Bayonetta 3's nnSdk 13.3.2) by name, read from the sdk's own proc table.

    python scripts/nvn_procs.py [targets/exefs/sdk.elf] [name ...]     prints "addr <hex> <name>" (sdklift_roots.txt lines)

NVN 13.3.2 keeps no "nvn..." strings: nvnDeviceGetProcAddress binary-searches a sorted blob of names without the "nvn"
prefix (u32 offsets into it, one per entry) and takes the function from a parallel array of pointers in data (relocated
R_AARCH64_RELATIVE). The search function is found by its shape: adrp+add of the names base and of the offset table,
"mov wN, #count - 1", and the pointer array's GOT slot. Nothing is hard-coded; docs/RE_NOTES.md "NVN" has the details.
"""
import os
import re
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


def _relative(d):
    _, dt = _layout(d)
    out = {}
    for i in range(dt[8] // 24):
        off, info, add = struct.unpack_from('<QQq', d, 0x1000 + dt[7] + i * 24)
        if info == 0x403:
            out[off] = add
    return out


def vtable(path, typeinfo_name):
    """Address of the method table (vtable + 0x10) of the class whose typeinfo name is `typeinfo_name`: the slot that
    points at its typeinfo after a non-relocated offset-to-top (a derived typeinfo points at it after its name)."""
    d = open(path, 'rb').read()
    rel = _relative(d)
    key = typeinfo_name.encode() + b'\0'                                # names can share a tail with the string before them
    names = {m.start() - 0x1000 for m in re.finditer(re.escape(key), d)}
    ti = [k - 8 for k, v in rel.items() if v in names]                   # typeinfo: {class_type_info vtable, name, ...}
    vt = [k for k, v in rel.items() if ti and v == ti[0] and k - 8 not in rel and 0 < rel.get(k + 8, 0) < _layout(d)[0]]
    if len(ti) != 1 or len(vt) != 1:
        raise SystemExit('%s: no single vtable for %s' % (path, typeinfo_name))
    return vt[0] + 8


def gpu_model(path):
    """[b3recomp] What nvn_sdk.c's device model points at, found by name or shape (docs/RE_NOTES.md section 6):
    the NVN format table (the GOT slot nvnTextureBuilderGetStorageAlignment loads; 0x30-byte records up to the next
    table, which holds pointers), the texture layout function
    (nvnTextureBuilderGetStorageSize's first call, also called by nvnTextureInitialize), the chip descriptor of the
    Tegra X1 (GPU id 0x12b, name "NVIDIA Tegra/Nintendo Switch") and the two vtables NvRmGpuDeviceChooseKind reaches."""
    d = open(path, 'rb').read()
    rel = _relative(d)
    fn = procs(path)['nvnTextureBuilderGetStorageAlignment']
    ws = struct.unpack_from('<64I', d, 0x1000 + fn)
    fmt = None
    for k in range(63):
        w, w2 = ws[k], ws[k + 1]
        if w & 0x9f000000 == 0x90000000 and w2 & 0xffc00000 == 0xf9400000 and (w2 >> 5) & 31 == w & 31:
            page = ((fn + 4 * k) & ~0xfff) + (sx((((w >> 5) & 0x7ffff) << 2) | ((w >> 29) & 3), 21) << 12)
            fmt = rel[page + ((w2 >> 10) & 0xfff) * 8]
            break
    name = d.find(b'\0NVIDIA Tegra/Nintendo Switch\0') + 1 - 0x1000
    desc = [k for k, v in rel.items() if v == name and struct.unpack_from('<I', d, 0x1000 + k + 0x10)[0] == 0x12b]
    if fmt is None or len(desc) != 1:
        raise SystemExit('%s: NVN format table or Tegra X1 descriptor not found' % path)
    size = procs(path)['nvnTextureBuilderGetStorageSize']
    bl = next(size + 4 * k + 4 * sx(w & 0x3ffffff, 26) for k, w in enumerate(struct.unpack_from('<32I', d, 0x1000 + size))
              if w >> 26 == 0x25)                                       # its first call: the texture layout block
    count = next(n for n in range(0x400) if any(fmt + 0x30 * n + k in rel for k in range(0, 0x30, 8)))  # next table: pointers
    return {'format_table': fmt, 'format_count': count, 'chip_descriptor': desc[0], 'texture_layout': bl,
            'vt_nvgpu_device': vtable(path, 'N8nvrm_gpu5nvgpu15NvRmNvgpuDeviceE'),
            'vt_hal_gm200': vtable(path, 'N8nvrm_gpu33NvRmGpu_HwHalFunctions_GM200_BaseE')}


if __name__ == '__main__':
    args = sys.argv[1:]
    path = args.pop(0) if args and args[0].endswith('.elf') else os.path.join(HERE, '..', 'targets', 'exefs', 'sdk.elf')
    table = procs(path)
    for name in args or table:
        print('addr %x %s' % (table[name], name) if table.get(name) else '# %s: not in this sdk' % name)
