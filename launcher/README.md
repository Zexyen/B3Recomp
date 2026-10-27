# Witch Time (launcher)

One window for setting up, building and playing: it checks your dump, runs the build with live progress, lists mods,
edits `config/graphics.ini`, and then starts the game in the same window. C++17, SDL3 and
[RmlUi](https://github.com/mikke89/RmlUi) on SDL's GPU API. Linux for now; the other platforms come later.

## Build

Needs CMake 3.20+, a C++17 compiler, SDL3 3.2 or newer (with its CMake config) and FreeType. RmlUi 6.3 is fetched by
CMake, or pass `-DRMLUI_SOURCE_DIR=<checkout>` to build without network access.

```
cmake -S launcher -B build/launcher -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/launcher
./build/witch-time
```

The program is written to `build/witch-time` and finds the repository from its own folder (or from `WITCHTIME_REPO`).
The screens are in `launcher/ui` (RML and RCSS) and are read at start, so they can be changed without rebuilding.

## How it works

- **Build.** v1 still runs `python3 scripts/setup.py` underneath (Python 3.10+ is needed as before) and reads its
  `[i/10]` step lines and `build/logs/compile.log` for progress. Stop sends SIGTERM; the next build resumes.
- **Mods.** Listed with `scripts/pchtxt.py tree`; switches are written to `config/mods.ini`. Code mods need a rebuild
  from the mods step, data mods apply at the next start.
- **Play in the same window.** The runtime link step also makes `build/libb3host.so`. The launcher loads it, hands its
  SDL window over (SDL global property `witchtime.window`, which `runtime/display.c` adopts) and calls `b3_host_main`.
  When the game exits through `ExitProcess`, `plat_exit_hook` starts the launcher again (`witch-time --returned
  <code>`), unless the player closed the window. If `libb3host.so` is missing or fails to load, the launcher hides its
  window and runs `build/b3-game` instead.
- **Input.** Mouse, keyboard (arrows, Enter, Escape) and controllers (d-pad or stick, A/Cross select, B/Circle back;
  left and right change a setting).

Fonts in `ui/fonts` are under the SIL Open Font License (the `OFL-*.txt` files next to them).
