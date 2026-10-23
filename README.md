# b3recomp

An unofficial static recompilation of *Bayonetta 3* (Nintendo Switch) for PC, in the spirit and architecture of
[B2recomp](../B2recomp): the game's AArch64 code is translated to C **on your PC from your own dump**, and runs on a
re-implemented system layer (threads, file system, input, audio, and NVN on Vulkan). Not an emulator, and not a way to
get the game.

**This repository contains no game code, no game assets, no Nintendo SDK code, no keys.** Everything derived from the
game (converted ELFs, translated C, the compiled `game.so`, caches) is generated locally into `targets/`, `build/`,
`cache/` and `runtime/gen/`, which git ignores. Never share them.

**Status: early, it does not show a picture yet.** It builds, and boots through the static constructors into `nnMain`,
mounts the RomFS and reads its archives, starts its threads and initialises NVN, then stops at texture setup.
Details: [docs/STATUS.md](docs/STATUS.md); findings: [docs/RE_NOTES.md](docs/RE_NOTES.md).

## Build (Linux)

Needs Python 3.10+ with `lz4` and `Pillow` (`unicorn` for the self-test), clang + lld, SDL3, Vulkan headers and loader,
glslang. Your dump: one folder with `exefs/` (`main`, `sdk`, `subsdk0`, `rtld`, `main.npdm`, plain NSO files) and
`romfs/` (the `.pkz` archives and `movie/`), extracted with your own tools from your own console and game; this project
decrypts nothing.

```
python scripts/setup.py --dump <your dump folder>
```

Steps and options are as in B2recomp's [BUILDING.md](../B2recomp/BUILDING.md) (minus the Windows launcher, icon and
game-specific patches). The `verify` step accepts only the build named in `scripts/setup.py`.

## Credits

Lifter, runtime and build scripts come from B2recomp (K0NL), which builds on
[ssbu-recomp](https://github.com/Denkishi/ssbu-recomp) (Denkishi). GPL-3.0; see `NOTICE.md` and `LICENSE`.
