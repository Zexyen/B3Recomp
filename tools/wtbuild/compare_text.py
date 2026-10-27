#!/usr/bin/env python3
"""Developer check: `wtbuild text` writes exactly the archive scripts/pc_settings_mod.py writes and says the same, on
made-up text files (random MCD message banks with random glyph sets, kerning and fonts; no game text) or on your own dump.

    python3 tools/wtbuild/compare_text.py [cases] [--seed N]
    python3 tools/wtbuild/compare_text.py --romfs <targets/romfs> --dump <witch-time-game --pkz-dump folder>
"""
import os
import random
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..', '..'))
sys.path.insert(0, os.path.join(ROOT, 'scripts'))
import pc_settings_mod as P  # noqa: E402

LATIN = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-+.()\'éèàäöüßçñ'
OTHER = 'АБВГДЕЖЗИКЛМНОПРСТУФабвгдежзиклмнопрстуфхцчшщыьэюя設定画面サイズスタート時に変更表示モード自動标准缩放其他固定显示器计时器動畫大小內部해상도다음시작설정자동기본고정확대'


def mcd(r, anchor):
    """a random MCD whose message `anchor` (hash) carries the font the rows take"""
    fonts = r.sample([3, 5, 7, 9, 11], r.randrange(1, 4))
    font = fonts[0]
    syms = []
    for f in fonts:
        chars = r.sample(LATIN, r.randrange(10, len(LATIN))) + r.sample(OTHER, r.randrange(0, len(OTHER)))
        syms += [(f, ord(c), r.getrandbits(16)) for c in chars]
    r.shuffle(syms)
    if r.random() < .3:
        syms += syms[:3]                                 # the same glyph twice: the first counts
    glyphs = [bytes(r.getrandbits(8) for _ in range(40)) for _ in range(r.randrange(1, 6))]
    fnts = [bytes(r.getrandbits(8) for _ in range(20)) for _ in range(len(fonts))]
    msgs, evs, off = [], [], 0
    names = ['MSG_%d' % i for i in range(r.randrange(3, 12))]
    for i, name in enumerate(names):
        pars = []
        for _ in range(r.randrange(1, 3)):
            f = font if r.random() < .7 else r.choice(fonts)
            ls = []
            for _ in range(r.randrange(1, 3)):
                us = []
                for _ in range(r.randrange(0, 12)):
                    if r.random() < .15:
                        us += [0x8001, r.choice([4, 6, 6, 8])]
                    else:
                        us += [r.randrange(len(syms)), r.choice([0, 1, 2, 2, 3, 0xfffe])]
                us.append(0x8000)
                if r.random() < .1:
                    us.append(0)                         # an odd count
                ls.append([us, r.choice([0, 0, 2]), r.choice([0, 0, 1]), struct.unpack('<f', struct.pack('<f', r.uniform(-50, 50)))[0],
                           r.choice([0.0, 12.5]), off])
                off += 1
            pars.append([r.randrange(100), r.randrange(100), f, ls])
        msgs.append([i + 1, zlib.crc32(name.lower().encode()) & 0x7fffffff, pars])
        evs.append((msgs[-1][1], i, name.encode()))
    # the anchor message, in the rows' font; sometimes a row's message is already there (it is then not added again)
    msgs.append([len(msgs) + 1, anchor, [[0, 0, font, [[[0, 1, 0x8000], 0, 0, 0.0, 0.0, off]]]]])
    evs.append((anchor, len(msgs) - 1, b'ANCHOR'))
    if r.random() < .2:
        name = r.choice(P.MENU + P.CORE)[0]
        h = zlib.crc32(name.lower().encode()) & 0x7fffffff
        msgs.append([len(msgs) + 1, h, [[0, 0, font, [[[0, 1, 0x8000], 0, 0, 0.0, 0.0, off + 1]]]]])
        evs.append((h, len(msgs) - 1, name.encode()))
    evs.sort(key=lambda e: e[0])
    return P.mcd_write(dict(msgs=msgs, syms=syms, glyphs=glyphs, fonts=fnts, evs=evs))


def dat(members):
    count = len(members)
    width = 16
    off_t = 0x20
    ext_t = off_t + 4 * count
    name_t = ext_t + 4 * count
    size_t = name_t + 4 + width * count
    data_at = (size_t + 4 * count + 15) & ~15
    out = bytearray(data_at)
    struct.pack_into('<4s6I', out, 0, b'DAT\0', count, off_t, ext_t, name_t, size_t, 0)
    struct.pack_into('<I', out, name_t, width)
    for i, (name, data) in enumerate(members):
        struct.pack_into('<I', out, off_t + 4 * i, len(out))
        struct.pack_into('<I', out, size_t + 4 * i, len(data))
        out[ext_t + 4 * i:ext_t + 4 * i + 4] = name.split('.')[-1].encode()[:3].ljust(4, b'\0')
        out[name_t + 4 + width * i:name_t + 4 + width * (i + 1)] = name.encode().ljust(width, b'\0')
        out += data + bytes(-len(data) % 16)
    return bytes(out)


def pkz(entries, align=0x1000):
    """entries: [(name, codec, data)]"""
    count = len(entries)
    names = bytearray(b'OodleKraken\0None\0')
    no = []
    for name, _, _ in entries:
        no.append(len(names))
        names += name.encode() + b'\0'
    table = bytearray(count * 0x20) + names
    pos = (0x20 + len(table) + align - 1) // align * align
    body = bytearray()
    for i, (name, codec, data) in enumerate(entries):
        struct.pack_into('<IIQQQ', table, i * 0x20, no[i], 12 if codec == 'None' else 0, len(data), pos + len(body), len(data))
        body += data + bytes(-len(data) % align)
    fsize = pos + len(body)
    head = struct.pack('<4sIQIIII', b'pkzl', 0x20000, fsize, count, 0x20, len(names), align)
    return head + table + bytes(pos - 0x20 - len(table)) + body


def make_case(r, romfs, dump):
    entries = [('ui/ui_other%d.dat' % k, r.choice(['None', 'OodleKraken']), bytes(r.getrandbits(8) for _ in range(r.randrange(1, 300))))
               for k in range(r.randrange(0, 4))]
    for l in P.LANGS:
        for k, anchor in (('menu', P.BRIGHTNESS), ('core', P.ON)):
            n = 'ui/ui_%s%s.dat' % (k, '_' + l if l else '')
            blob = dat([('mess%s.mcd' % k, mcd(r, anchor)), ('other.bin', bytes(r.getrandbits(8) for _ in range(r.randrange(1, 64))))])
            p = os.path.join(dump, n)
            os.makedirs(os.path.dirname(p), exist_ok=True)
            with open(p, 'wb') as f:
                f.write(blob)
            entries.append((n, 'OodleKraken', bytes(r.getrandbits(8) for _ in range(r.randrange(10, 200)))))   # compressed: not read
    r.shuffle(entries)
    os.makedirs(romfs, exist_ok=True)
    with open(os.path.join(romfs, 'ui_000.pkz'), 'wb') as f:
        f.write(pkz(entries, r.choice([0x10, 0x1000])))


def compare(romfs, dump, t, exe):
    outs = []
    for side, cmd in (('py', [sys.executable, os.path.join(ROOT, 'scripts', 'pc_settings_mod.py')]), ('rs', [exe, 'text'])):
        out = os.path.join(t, 'mods_' + side)
        os.makedirs(os.path.join(out, P.OLD_MOD), exist_ok=True)       # removed by both
        p = subprocess.run(cmd + ['--romfs', romfs, '--dump', dump, '--out', out], capture_output=True, text=True)
        arc = os.path.join(out, P.MOD, 'romfs', 'ui_000.pkz')
        outs.append((p.returncode, p.stdout.replace(out, 'OUT'), open(arc, 'rb').read() if os.path.exists(arc) else None,
                     os.path.exists(os.path.join(out, P.OLD_MOD)), p.stderr))
    (a, b) = outs
    if a[:4] != b[:4]:
        print('differs:\n--- pc_settings_mod.py (exit %d)\n%s%s\n--- wtbuild (exit %d)\n%s%s\narchives %s' % (
            a[0], a[1][-2000:], a[4][-1500:], b[0], b[1][-2000:], b[4][-1500:], 'same' if a[2] == b[2] else 'differ'))
        return False
    return True


def main():
    args = sys.argv[1:]
    exe = os.path.join(HERE, 'target', 'release', 'wtbuild')
    t = tempfile.mkdtemp(prefix='wttext_')
    try:
        if '--romfs' in args:
            ok = compare(os.path.abspath(args[args.index('--romfs') + 1]), os.path.abspath(args[args.index('--dump') + 1]), t, exe)
            print('text: %s' % ('the same archive and output' if ok else 'DIFFERS'))
            return 0 if ok else 1
        seed = int(args[args.index('--seed') + 1]) if '--seed' in args else 1
        cases = int(args[0]) if args and args[0].isdigit() else 20
        r = random.Random(seed)
        bad = 0
        for i in range(cases):
            d = os.path.join(t, 'c%d' % i)
            make_case(r, os.path.join(d, 'romfs'), os.path.join(d, 'dump'))
            if not compare(os.path.join(d, 'romfs'), os.path.join(d, 'dump'), d, exe):
                bad += 1
                if bad >= 3:
                    break
            shutil.rmtree(d)
        print('text: %d cases, %d differ' % (cases, bad))
        return 1 if bad else 0
    finally:
        shutil.rmtree(t, ignore_errors=True)


if __name__ == '__main__':
    sys.exit(main())
