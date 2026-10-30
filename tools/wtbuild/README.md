# wtbuild

The Witch Time build in one program: from your own dump to `build/witch-time-game`, with no Python. It is a port of
`scripts/setup.py` and the scripts it runs, for the Linux toolchain:

| step | was | here |
|---|---|---|
| deps | setup.py `step_deps` | `src/setup.rs` (also checks the libraries the runtime needs) |
| verify, convert | setup.py, `tools/extract/nso2elf.py` | `src/setup.rs`, `src/nso.rs` |
| mods | `scripts/pchtxt.py` | `src/pchtxt.rs` |
| tables | `runtime/gen/make_gen.py`, `lifter/sdklift.py`, `scripts/nvn_procs.py` | `src/tables.rs`, `src/sdklift.rs`, `src/nvn_procs.rs` (+ `runtime/gen_static/`) |
| lift | `lifter/recomp.py`, `lifter/recomp_eh.py` | `src/lifter/` |
| compile | `scripts/compile_main.py` | `src/compile.rs` |
| link | `scripts/build_runtime.py` | `src/link.rs` (split differently: "Prebuilt runtime" below) |
| text | `scripts/pc_settings_mod.py`, `pkz.py`, `pgdat.py` | `src/text.rs` |
| check | `lifter/test_ext.py` (Unicorn) | skipped: a developer's check, still in Python |

Every file it writes is byte for byte the one the Python wrote, and its output lines, `build/setup_state.json` and
`build/logs/` are the same. Two things differ on purpose: the lifted code is compiled against `runtime/freestanding`
instead of the system's C library headers (the same objects, checked by `compare_freestanding.py`; a build made with
`setup.py` is compiled once more because the compile flags changed), and the link step splits the runtime from the
player's tables (below).

```
cargo build --release --manifest-path tools/wtbuild/Cargo.toml
tools/wtbuild/target/release/wtbuild setup --dump <your dump folder> [--jobs N] [--only <step> | --from <step>]
                                           [--status] [--accept-modified] [--mods <folder>] [--llvm <folder>]
```

The launcher runs `build/wtbuild` (a release) or `tools/wtbuild/target/release/wtbuild` (a checkout), and falls back to
`python3 scripts/setup.py` when neither is there.

Other commands, mostly for developers: `nso2elf <in> <out>`, `lift <main.elf> <outdir> [--no-patches]`,
`tables <repo>`, `compile [--jobs N]`, `link`, `runtime --out <dir>` (the prebuilt runtime, CI), `text [--romfs <folder>] [--dump <folder>] [--out <mods folder>]`,
`mods-tree <mods folder> [<main NSO>] [--ini=<mods.ini>]` (the launcher's Mods window), `mods-apply`, `lift-words`, `sha256 <file>` (the launcher's updater).
`--repo <folder>` names the checkout (default: the current folder).

## Prebuilt runtime

The runtime (`libwitchtime.so`, which the launcher loads, `witch-time-game`, which runs it as a program of its own,
and `libssbu_glslang.so`) holds nothing from the dump: the numbers it needs from the player's `main.elf` and `sdk.elf`
are `extern const` variables there (`runtime/split`), defined in `game.so` by `runtime/wt_tables.c` together with the
dump's sdk tables (`runtime/gen/sdkdata.c`, `sdklift_gen.c`). So the release workflow builds it once
(`wtbuild runtime --out prebuilt`, against its own SDL3, FFmpeg 7.1 and a static glslang) and ships it in `prebuilt/`
with a `manifest.json` that records the hash of the sources it was built from (`runtime/` but `gen/`, and
`lifter/sdklift_install.txt`). The link step copies it into `build/` when that hash matches and builds the runtime
itself otherwise (a checkout, or a release whose `runtime/` was edited), so changing the runtime works as before.

What the player's PC compiles (the lifted code and those three files) uses only `runtime/freestanding` and clang's own
headers, and `game.so` links with `-nostdlib` and the compiler's builtins: no libc development package, no system
libraries. With the bundled compiler (`deps/llvm`, clang 23 and lld from LLVM's release, found before the system's)
a player installs nothing to build. `smoke_link.py` runs the tables, lift, compile and link steps on stand-in modules,
checks that every symbol resolves and runs `witch-time-game` until it has mapped the image; the release workflow runs
it on the runtime it ships.

### Windows

The same split, with Windows' files: the runtime is one program, `witch-time-game.exe`, built with llvm-mingw
(`wtbuild runtime --target windows --out prebuilt`, which CI runs on Linux), and the player's PC links `game.dll`. The
program imports the tables from `game.dll` through `libgame.dll.a` (made by llvm-dlltool from the export list,
`GAME_EXPORTS` in `src/link.rs`); `game.dll` imports nothing from the program, and its calls back into the runtime go
through the function pointers `game_bind` hands it (`runtime/game_shim.c`). `game.dll` links with `-nostdlib` and its
own entry point, takes its C functions from `ucrtbase.dll` (mingw-w64's `libucrtbase.a`, shipped in `prebuilt/`) and
llvm-mingw's builtins. Every clang gets `--target=x86_64-w64-mingw32` (`src/target.rs`), so the bundled compiler is
LLVM's own Windows release plus those builtins in `lib/clang/23/lib/windows/`. The `deps` step checks the DLLs the
program imports (its PE import table) against `build\`, `prebuilt\` and System32. `smoke_link.py --target windows`
runs the smoke test there, or on Linux under Wine; `compare_freestanding.py --target windows` checks the freestanding
headers against mingw-w64's.

`scripts/build_runtime.py` (the Python reference) still links everything into one program.

### Android

The same split again, for AArch64 Android 10 and later (`--target=aarch64-linux-android29`), in progress: the runtime
is `libwitchtime.so` with `SDL_main` inside (`runtime/wt_main.c`: the app's SDLActivity loads it, and the player's
files are in the app's external storage folder), built with the NDK's clang (`LLVM_ROOT=<ndk>/toolchains/llvm/prebuilt/
linux-x86_64 wtbuild runtime --target android --out prebuilt-android`, after copying the Android builds of SDL3 and
FFmpeg into that folder: the link looks for them there, the NDK's clang ignores `LIBRARY_PATH`). The player's PC
compiles the lifted code with its bundled clang and no NDK (`wtbuild compile --target android`, objects in
`build/obj_android`; `runtime/freestanding/setjmp.h` has bionic's `jmp_buf`) and links `game.so`
(`wtbuild link --target android`, into `build/android/`) from `prebuilt-android/`, with the compiler-rt builtins and the NDK's stand-ins for
bionic (`libc.so`, `libm.so`, `libdl.so`, symbol tables only) the runtime build copied there: the bundled clang has
neither for that target, and `game.so` must name bionic as its need, since Android's linker resolves every symbol of
`game.so` when the app loads it (before `libwitchtime.so`, which needs `game.so` and finds it loaded; the three SDK
calls of the lifted sdk go through `game_bind`'s pointers there, as on Windows). The app is `android/` (README there);
CI's job `android` builds SDL3, FFmpeg and glslang for Android (`android/build_deps.sh`), the runtime and the app. Not yet
run on a device. The runtime's Android fixes are `#ifdef __ANDROID__`: fibers switched by a few lines of
assembly (bionic has no `makecontext`), no `backtrace()` or sampling profiler, a GL 1.1 header and empty functions of
its own (`runtime/posix/GL/gl_android.h`, `gl_android.c`: the NDK has no desktop OpenGL; the renderer there is Vulkan).

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
| `compare_freestanding.py [--words N] [--gen <dir>]` | lifted code (random words, or your `build/gen_eh`) compiled with `runtime/freestanding` and with the system's headers | identical objects (clang 18 and 23) |

`--main`, `--exefs`, `compare_text.py --romfs ... --dump ...` and `compare_freestanding.py --gen build/gen_eh --tables runtime/gen`
run the same checks on your own dump, on your PC.
