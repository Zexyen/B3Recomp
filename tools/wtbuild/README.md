# wtbuild

The Witch Time build in one program: from your own dump to `build/witch-time-game`, with no Python. It is a port of
`scripts/setup.py` and the scripts it runs, for the Linux toolchain:

| step | was | here |
|---|---|---|
| deps | setup.py `step_deps` | `src/setup.rs` (also checks the system libraries the link needs) |
| verify, convert | setup.py, `tools/extract/nso2elf.py` | `src/setup.rs`, `src/nso.rs` |
| mods | `scripts/pchtxt.py` | `src/pchtxt.rs` |
| tables | `runtime/gen/make_gen.py`, `lifter/sdklift.py`, `scripts/nvn_procs.py` | `src/tables.rs`, `src/sdklift.rs`, `src/nvn_procs.rs` (+ `runtime/gen_static/`) |
| lift | `lifter/recomp.py`, `lifter/recomp_eh.py` | `src/lifter/` |
| compile | `scripts/compile_main.py` | `src/compile.rs` |
| link | `scripts/build_runtime.py` | `src/link.rs` |
| text | `scripts/pc_settings_mod.py`, `pkz.py`, `pgdat.py` | `src/text.rs` |
| check | `lifter/test_ext.py` (Unicorn) | skipped: a developer's check, still in Python |

Every file it writes is byte for byte the one the Python wrote, and its output lines, `build/setup_state.json`,
`build/logs/` and the compile step's up-to-date record (`build/obj/_stamps.json`) are the same, so a build made with
`setup.py` is taken over without compiling again (the first run after the switch redoes tables, lift, link and text,
because the fingerprint of the build's own sources now includes this program).

```
cargo build --release --manifest-path tools/wtbuild/Cargo.toml
tools/wtbuild/target/release/wtbuild setup --dump <your dump folder> [--jobs N] [--only <step> | --from <step>]
                                           [--status] [--accept-modified] [--mods <folder>] [--llvm <folder>]
```

The launcher runs `build/wtbuild` (a release) or `tools/wtbuild/target/release/wtbuild` (a checkout), and falls back to
`python3 scripts/setup.py` when neither is there. The Windows toolchains (mingw, msvc) are still `scripts/setup.py`'s.

Other commands, mostly for developers: `nso2elf <in> <out>`, `lift <main.elf> <outdir> [--no-patches]`,
`tables <repo>`, `compile [--jobs N]`, `text [--romfs <folder>] [--dump <folder>] [--out <mods folder>]`,
`mods-tree <mods folder> [<main NSO>] [--ini=<mods.ini>]` (the launcher's Mods window), `mods-apply`, `lift-words`.
`--repo <folder>` names the checkout (default: the current folder).

## Checked against the Python

The Python scripts stay in the repository as the reference. Each `compare_*.py` here runs both on the same input and
compares every output byte; the stand-in programs are built from `testdata/fake_main/` (no game or sdk code; needs clang
and ld.lld with the AArch64 target and Python's `lz4`).

| check | what | result when ported |
|---|---|---|
| `compare_lifter.py [N] [--seed S]` | single-instruction lifts of random words against `recomp.py` | 5 x 1M words, 0 differ |
| `compare_lift.py [--main <NSO>]` | NSO conversion and the whole-program lift (EH tables, landing pads, buckets) | identical ELF and C files |
| `compare_tables.py [--exefs <dir>]` | every file of `runtime/gen/` and `sdk.elf`, with an NVN-shaped proc table, getter switch, vtables and format table | 14 files identical |
| `compare_mods.py [N] [--seed S]` | random mod folders: tree lines, apply output and patched ELF, errors included | 900 folders, 0 differ |
| `compare_text.py [N] [--seed S]` | made-up message banks: the PC settings archive | 50 archives identical |

`--main`, `--exefs` and `compare_text.py --romfs ... --dump ...` run the same checks on your own dump, on your PC.
