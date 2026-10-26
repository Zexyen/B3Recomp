#!/usr/bin/env python3
"""The label of the "PC Settings" row of the game's Options screen (runtime/options_row.c): a RomFS mod made from YOUR dump.

    python scripts/pc_settings_mod.py [--romfs <folder>] [--dump <folder>] [--out <mods folder>]

The Options screen's labels are messages of ui/ui_menu[_<lang>].dat -> messmenu.mcd (PlatinumGames MCD, docs/RE_NOTES.md
section 8). This adds one message, MENU_OPTION_LIST_PC_SETTINGS, to the 13 language files (spelled with each file's own
glyphs), and writes <out>/b3recomp-pc-settings/romfs/ui_000.pkz: the dump's archive byte for byte, with those 13 entries
appended stored ("None") and their entry records pointed at the copies. The decompressed entries come from --dump (a
`b3-game --pkz-dump` folder), else from build/b3-game --pkz-dump into a temporary folder.
Defaults: --romfs targets/romfs, --out mods (beside build/, where the runtime looks). The output holds the game's files:
it stays on your PC (git ignores mods/); never share it.
"""
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(REPO, 'scripts'))
import pgdat  # noqa: E402
import pkz    # noqa: E402

NAME = 'MENU_OPTION_LIST_PC_SETTINGS'
HASH = zlib.crc32(NAME.lower().encode()) & 0x7fffffff      # 0x1a9a646f: runtime/options_row.c PC_LABEL
BRIGHTNESS = 0x5f236d04                                      # MENU_OPTION_LIST_016: its font is the list's
LABELS = {'': 'PC設定', 'us': 'PC Settings', 'gb': 'PC Settings', 'nl': 'PC Settings', 'pt': 'PC Settings',
          'de': 'PC-Einstellungen', 'fr': 'Paramètres PC', 'es': 'Ajustes de PC', 'it': 'Impostazioni PC',
          'ru': 'Настройки ПК', 'cn': 'PC设置', 'tw': 'PC設定', 'kr': 'PC 설정'}
MOD = 'b3recomp-pc-settings'


# ---- MCD: header {u32 offset, count} x 5 (messages, symbols, glyphs, fonts, events); message {paragraphs, count, seq,
# event hash}; paragraph {lines, count, v, h, font}; line {text, 0, units, units, below, horiz}; text = u16 pairs
# (symbol, kerning), 0x8001 <width> = a space, 0x8000 = end; symbol {u16 font, u16 char, u32 glyph}; event {hash, message,
# char name[32]} sorted by hash. Sections in this order, each but the last followed by 4 zero bytes (the text by 1-4, to a
# multiple of 4).
def mcd_read(d):
    mo, mc, so, sc, go, gc, fo, fc, eo, ec = struct.unpack_from('<10I', d, 0)
    msgs = []
    for i in range(mc):
        po, pc, seq, ev = struct.unpack_from('<4I', d, mo + 16 * i)
        pars = []
        for j in range(pc):
            lo, lc, vp, hp, font = struct.unpack_from('<5I', d, po + 20 * j)
            lines = []
            for k in range(lc):
                off, pad, n, n2, below, horiz = struct.unpack_from('<4I2f', d, lo + 24 * k)
                lines.append([list(struct.unpack_from('<%dH' % n, d, off)), pad, n2 - n, below, horiz, off])
            pars.append([vp, hp, font, lines])
        msgs.append([seq, ev, pars])
    return dict(msgs=msgs, syms=[struct.unpack_from('<2HI', d, so + 8 * i) for i in range(sc)],
                glyphs=[d[go + 40 * i:go + 40 * i + 40] for i in range(gc)], fonts=[d[fo + 20 * i:fo + 20 * i + 20] for i in range(fc)],
                evs=[struct.unpack_from('<II32s', d, eo + 40 * i) for i in range(ec)])


def mcd_write(m):
    lines = [ln for msg in m['msgs'] for p in msg[2] for ln in p[3]]
    out = bytearray(40)
    toff = {}
    for i in sorted(range(len(lines)), key=lambda i: lines[i][5]):     # texts in their original order, new ones last
        toff[i] = len(out)
        out += struct.pack('<%dH' % len(lines[i][0]), *lines[i][0])
    out += bytes(((len(out) + 4) & ~3) - len(out))
    npar = sum(len(msg[2]) for msg in m['msgs'])
    mo = len(out); out += bytes(16 * len(m['msgs']) + 4)
    po = len(out); out += bytes(20 * npar + 4)
    lo = len(out); out += bytes(24 * len(lines) + 4)
    pi = li = 0
    for i, (seq, ev, pars) in enumerate(m['msgs']):
        struct.pack_into('<4I', out, mo + 16 * i, po + 20 * pi, len(pars), seq, ev)
        for vp, hp, font, ls in pars:
            struct.pack_into('<5I', out, po + 20 * pi, lo + 24 * li, len(ls), vp, hp, font)
            pi += 1
            for us, pad, extra, below, horiz, _ in ls:
                struct.pack_into('<4I2f', out, lo + 24 * li, toff[li], pad, len(us), len(us) + extra, below, horiz)
                li += 1
    heads = []
    for key, fmt in (('syms', '<2HI'), ('glyphs', None), ('fonts', None), ('evs', '<II32s')):
        heads += [len(out), len(m[key])]
        for x in m[key]:
            out += struct.pack(fmt, *x) if fmt else x
        out += bytes(4 if key != 'evs' else 0)
    struct.pack_into('<10I', out, 0, mo, len(m['msgs']), *heads)
    return bytes(out)


def add_label(d, text):
    """d (a messmenu.mcd) with the message NAME = text, in the font of the list's labels"""
    m = mcd_read(d)
    assert mcd_write(m) == d, 'messmenu.mcd: this layout is not the one known (the file would not be rebuilt as it is)'
    if any(e[0] == HASH for e in m['evs']):
        return d
    syms = m['syms']
    msg = m['msgs'][next(i for h, i, _ in m['evs'] if h == BRIGHTNESS)]
    font = msg[2][0][2]
    kern, kern1, space = {}, {}, {}
    for _, _, pars in m['msgs']:                             # kerning after a symbol (before the next), space widths, in this font
        for _, _, f, ls in pars:
            for us, *_ in ls:
                if f != font:
                    continue
                pairs = [(us[i], us[i + 1]) for i in range(0, len(us) - 1, 2)]
                for (a, ka), (b, _) in zip(pairs, pairs[1:] + [(0x8000, 0)]):
                    if a == 0x8001:
                        space[ka] = space.get(ka, 0) + 1
                    elif a < 0x8000:
                        kern[a, b] = ka
                        kern1.setdefault(a, {}).setdefault(ka, 0)
                        kern1[a][ka] += 1

    def sym(ch):
        for f in (font, 7):                                  # 7: the Latin font, for "PC" in the Chinese / Korean files
            s = next((i for i, (sf, c, _) in enumerate(syms) if sf == f and c == ord(ch)), None)
            if s is not None:
                return s
        raise ValueError('no glyph for %r in this messmenu.mcd' % ch)
    seq = [0x8001 if ch == ' ' else sym(ch) for ch in text] + [0x8000]
    us = []
    for a, b in zip(seq, seq[1:] + [None]):
        if a == 0x8000:
            us.append(a)
        elif a == 0x8001:
            us += [a, max(space, key=space.get) if space else 7]
        else:
            k = kern.get((a, b), max(kern1[a], key=kern1[a].get) if a in kern1 else 0)
            us += [a, k]
    m['msgs'].append([len(m['msgs']) + 1, HASH, [[0, 0, font, [[us, 0, 0, 0.0, 0.0, 1 << 40]]]]])
    m['evs'].append((HASH, len(m['msgs']) - 1, NAME.encode()))
    m['evs'].sort(key=lambda e: e[0])
    return mcd_write(m)


def dat_replace(blob, member, data):
    """the DAT with `member` appended at the end (16-byte aligned) and its offset / size pointed there"""
    _, count, off_t, _, _, size_t, _ = struct.unpack_from('<4s6I', blob, 0)
    i = next(k for k, (name, _, _, _) in enumerate(pgdat.entries(blob)) if name == member)
    out = bytearray(blob) + bytes(-len(blob) % 16)
    struct.pack_into('<I', out, off_t + 4 * i, len(out))
    struct.pack_into('<I', out, size_t + 4 * i, len(data))
    out += data + bytes(-len(data) % 16)
    return bytes(out)


def main(argv):
    args = dict(zip(argv[::2], argv[1::2]))
    if set(args) - {'--romfs', '--dump', '--out'} or len(argv) % 2:
        sys.exit(__doc__)
    romfs = args.get('--romfs', os.path.join(REPO, 'targets', 'romfs'))
    arc = os.path.join(romfs, 'ui_000.pkz')
    out = os.path.join(args.get('--out', os.path.join(REPO, 'mods')), MOD, 'romfs', 'ui_000.pkz')
    fsize, entries = pkz.read_pkz(arc)
    names = ['ui/ui_menu%s.dat' % ('_' + t if t else '') for t in LABELS]
    assert all(any(e[0] == n for e in entries) for n in names), 'ui_000.pkz: not the archive known'
    dump, tmp = args.get('--dump'), None
    if not dump:
        tmp = dump = tempfile.mkdtemp(prefix='b3pcmod_')
        exe = os.path.join(REPO, 'build', 'b3-game' + ('.exe' if os.name == 'nt' else ''))
        r = subprocess.run([exe, os.path.join(REPO, 'targets', 'exefs', 'main.elf'), '--pkz-dump', tmp, 'ui/ui_menu'],
                           cwd=os.path.join(REPO, 'build'), env=dict(os.environ, SSBU_ROMFS=romfs), capture_output=True, text=True)
        if r.returncode:
            sys.exit('b3-game --pkz-dump failed:\n' + r.stdout[-2000:] + r.stderr[-2000:])
    try:
        new = {}
        for n, t in zip(names, LABELS.values()):
            blob = open(os.path.join(dump, n), 'rb').read()
            mcd = next(blob[o:o + s] for name, _, o, s in pgdat.entries(blob) if name == 'messmenu.mcd')
            new[n] = dat_replace(blob, 'messmenu.mcd', add_label(mcd, t))
    finally:
        if tmp:
            shutil.rmtree(tmp)
    with open(arc, 'rb') as f:
        head = f.read(0x20)
        _, _, _, count, toff, nsize, align = struct.unpack_from('<4sIQIIII', head)
        f.seek(toff)
        table = bytearray(f.read(count * 0x20 + nsize))
    none = bytes(table[count * 0x20:]).index(b'None\0')
    assert none == 0 or table[count * 0x20 + none - 1] == 0
    os.makedirs(os.path.dirname(out), exist_ok=True)
    shutil.copyfile(arc, out + '.tmp')
    pos = (fsize + align - 1) // align * align
    with open(out + '.tmp', 'r+b') as f:
        for i, e in enumerate(entries):
            if e[0] in new:
                d = new[e[0]]
                no, = struct.unpack_from('<I', table, i * 0x20)
                struct.pack_into('<IIQQQ', table, i * 0x20, no, none, len(d), pos, len(d))
                f.seek(pos)
                f.write(d)
                pos = (pos + len(d) + align - 1) // align * align
        f.truncate(pos)
        f.seek(0)
        f.write(struct.pack('<4sIQIIII', b'pkzl', 0x20000, pos, count, toff, nsize, align) + table)
    os.replace(out + '.tmp', out)
    print('%s: %d bytes, %d language files with the "PC Settings" label (message %s, 0x%08x)' % (out, pos, len(new), NAME, HASH))


if __name__ == '__main__':
    main(sys.argv[1:])
