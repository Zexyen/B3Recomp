# Witch Time

An unofficial static recompilation of *Bayonetta 3* (Nintendo Switch) for PC, in the spirit and architecture of
[B2recomp](../B2recomp): the game's AArch64 code is translated to C **on your PC from your own dump**, and runs on a
re-implemented system layer (threads, file system, input, audio, and NVN on Vulkan). Not an emulator, and not a way to
get the game.

**This repository contains no game code, no game assets, no Nintendo SDK code, no keys.** Everything derived from the
game (converted ELFs, translated C, the compiled `game.so`, caches) is generated locally into `targets/`, `build/`,
`cache/` and `runtime/gen/`, which git ignores. Never share them.

## Status

| What | Result |
|---|---|
| Boot and gameplay | boots and plays; 60 fps in play (Tokyo and Shibuya tested), occasional short hitches |
| Menus and cutscenes | 30 fps as on the console by default; 60 fps with `cutscene_fps = 60` in `config/graphics.ini` (timing stays real time; movies keep their 30 fps) |
| Resolution | the game's 1600x900 by default; `resolution_scale` in `config/graphics.ini` renders it larger (1.2 = 1080p, 1.6 = 1440p, 2.4 = 4K, measured at 60 fps on an RX 9070 XT; the first run at a new scale has about two minutes of held loading screens once) |
| Input | controllers, and keyboard + mouse (rebindable in `config/controllers.ini`; mouse camera; both at once); keyboard tested with synthetic events, not yet by hand |
| Settings | in-game PC settings panel: F1 or L3 + R3 (the game is held while it is open); vsync, cutscene frame rate, window, anisotropic filtering, texture detail and aspect change at once, the internal resolution at the next start; written to `config/graphics.ini` with its comments kept. The same settings (internal resolution 1.0-2.4, cutscene frame rate, vsync, display mode, anisotropic filtering, texture detail, aspect) are also rows of the game's own Options screen (Screen Display page; changed with left / right, X resets a row), with a "PC Settings" row that opens the panel; labelled in each of the game's languages (setup step `text` makes that text from your dump). Not a tab of their own: see STATUS.md |

Details: [docs/STATUS.md](docs/STATUS.md); findings: [docs/RE_NOTES.md](docs/RE_NOTES.md).

## Build (Linux)

Needs clang + lld and the development files of SDL3, Vulkan (headers and loader), glslang, FFmpeg, OpenSSL and OpenGL
(the `deps` step checks them and names what is missing). No Python. Your dump: one folder with `exefs/` (`main`, `sdk`,
`subsdk0`, `rtld`, `main.npdm`, plain NSO files) and `romfs/` (the `.pkz` archives and `movie/`), extracted with your
own tools from your own console and game; this project decrypts nothing.

The build is one program, `wtbuild` (`tools/wtbuild`, Rust). A release ships it built as `build/wtbuild`; in a checkout,
build it once with cargo:

```
cargo build --release --manifest-path tools/wtbuild/Cargo.toml
tools/wtbuild/target/release/wtbuild setup --dump <your dump folder>
```

Or build the launcher (`launcher/README.md`) and run `build/witch-time`, which does the same with progress on screen
and then plays in its own window.

The steps (`deps verify convert mods tables lift compile link text check`, resumable; `--only`, `--from`, `--status`)
are those of `scripts/setup.py`, which stays for the Windows toolchains and as the reference the Rust port is checked
against (`tools/wtbuild/README.md`). The `verify` step accepts only the build named in `tools/wtbuild/src/setup.rs`.

## Credits

Lifter, runtime and build scripts come from B2recomp (K0NL), which builds on
[ssbu-recomp](https://github.com/Denkishi/ssbu-recomp) (Denkishi). GPL-3.0; see `NOTICE.md` and `LICENSE`.
