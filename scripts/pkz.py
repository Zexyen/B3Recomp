"""Read PlatinumGames' .pkz archives (Bayonetta 3 RomFS). Reads YOUR dump only; writes nothing into it.

    python scripts/pkz.py list <file.pkz>                      entries: codec, size, stored size, offset, name
    python scripts/pkz.py check <romfs folder>                  parse every .pkz, check every entry lies inside its file
    python scripts/pkz.py extract <file.pkz> <out dir> [--oodle <lib>] [names...]

Layout (little endian; docs/RE_NOTES.md "pkz"):
    0x00 'pkzl'   0x04 u32 version 0x20000   0x08 u64 file size   0x10 u32 entry count   0x14 u32 entry table offset (0x20)
    0x18 u32 size of the name table that follows the entries   0x1c u32 0x1000 (alignment of the data area, not checked)
    entry (0x20 bytes): u32 name offset, u32 codec name offset (both into the name table), u64 size, u64 data offset,
                        u64 stored size
Codec names seen: "None" (stored) and "OodleKraken" (one Oodle stream per entry). Oodle is not open source: the game has
its own decoder linked into main, so the recompiled game decompresses its files itself. This tool decompresses Kraken
entries only with an Oodle library you already have (--oodle <path to liboo2corelinux64.so / oo2core_*_win64.dll>); without
one it extracts the stored entries and says which ones it skipped.
"""
import ctypes
import os
import struct
import sys


def read_pkz(path):
    """[(name, codec, size, offset, stored_size)] of one archive."""
    with open(path, 'rb') as f:
        head = f.read(0x20)
        magic, ver, fsize, count, toff, nsize, _align = struct.unpack_from('<4sIQIIII', head)
        if magic != b'pkzl':
            raise ValueError('%s: not a pkz archive (magic %r)' % (path, magic))
        if ver != 0x20000:
            raise ValueError('%s: unknown pkz version 0x%x' % (path, ver))
        f.seek(toff)
        table = f.read(count * 0x20 + nsize)
    names = table[count * 0x20:]

    def name(o):
        return names[o:names.index(b'\0', o)].decode('utf-8')

    out = []
    for i in range(count):
        no, co, size, off, csize = struct.unpack_from('<IIQQQ', table, i * 0x20)
        out.append((name(no), name(co), size, off, csize))
    return fsize, out


def check(root):
    n_files = n_entries = 0
    codecs = {}
    for fn in sorted(os.listdir(root)):
        if not fn.endswith('.pkz'):
            continue
        p = os.path.join(root, fn)
        fsize, entries = read_pkz(p)
        assert fsize == os.path.getsize(p), '%s: header says %d bytes, file has %d' % (fn, fsize, os.path.getsize(p))
        for name, codec, size, off, csize in entries:
            assert off + csize <= fsize, '%s: %s runs past the end' % (fn, name)
            assert codec != 'None' or size == csize, '%s: stored entry %s with size %d != %d' % (fn, name, size, csize)
            codecs[codec] = codecs.get(codec, 0) + 1
        n_files += 1
        n_entries += len(entries)
    print('%d archives, %d entries, codecs: %s' % (n_files, n_entries, ', '.join('%s %d' % kv for kv in sorted(codecs.items()))))


def oodle(lib):
    dll = ctypes.CDLL(lib)
    fn = dll.OodleLZ_Decompress
    fn.restype = ctypes.c_ssize_t
    fn.argtypes = [ctypes.c_void_p, ctypes.c_ssize_t, ctypes.c_void_p, ctypes.c_ssize_t] + [ctypes.c_int] * 3 + \
                  [ctypes.c_void_p, ctypes.c_ssize_t, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_ssize_t, ctypes.c_int]

    def dec(src, size):
        out = ctypes.create_string_buffer(size)
        got = fn(src, len(src), out, size, 1, 0, 0, None, 0, None, None, None, 0, 3)
        if got != size:
            raise ValueError('Oodle returned %d of %d bytes' % (got, size))
        return out.raw
    return dec


def extract(path, outdir, lib, only):
    _, entries = read_pkz(path)
    dec = oodle(lib) if lib else None
    skipped = 0
    with open(path, 'rb') as f:
        for name, codec, size, off, csize in entries:
            if only and name not in only:
                continue
            # entry names are relative paths inside the archive; refuse anything that would leave outdir
            dst = os.path.normpath(os.path.join(outdir, name))
            if os.path.isabs(name) or not dst.startswith(os.path.normpath(outdir) + os.sep):
                raise ValueError('unsafe entry name %r' % name)
            f.seek(off)
            data = f.read(csize)
            if codec == 'OodleKraken':
                if not dec:                              # shortcut: no Kraken decoder of our own; the game has one (main)
                    skipped += 1
                    continue
                data = dec(data, size)
            elif codec != 'None':
                raise ValueError('%s: unknown codec %r' % (name, codec))
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            with open(dst, 'wb') as o:
                o.write(data)
    if skipped:
        print('%d Kraken entries skipped (no --oodle library given)' % skipped)


def main(argv):
    if len(argv) >= 2 and argv[0] == 'list':
        fsize, entries = read_pkz(argv[1])
        for name, codec, size, off, csize in entries:
            print('%-11s %10d %10d  0x%09x  %s' % (codec, size, csize, off, name))
        print('%d entries, %d bytes' % (len(entries), fsize))
    elif len(argv) >= 2 and argv[0] == 'check':
        check(argv[1])
    elif len(argv) >= 3 and argv[0] == 'extract':
        rest, lib = argv[3:], None
        if '--oodle' in rest:
            i = rest.index('--oodle')
            lib, rest = rest[i + 1], rest[:i] + rest[i + 2:]
        extract(argv[1], argv[2], lib, set(rest))
    else:
        sys.exit(__doc__)


if __name__ == '__main__':
    main(sys.argv[1:])
