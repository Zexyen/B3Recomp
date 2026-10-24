# b3recomp status

Stage 2 (2026-10-23): boots to the title screen, the menus and the prologue's pause menu; the 3D world is black. Linux only (the system's clang 23, SDL3, Vulkan, glslang), measured on the developer's PC
(16 threads, RX 9070 XT / RADV) with their own dump.

## What builds

`python scripts/setup.py --dump <dump>` runs all nine steps and they pass:

| step | result |
|---|---|
| deps, verify, convert, mods | the dump is the expected build (main build id `b47cc79b...`); main, sdk, subsdk0 -> ELF, segment hashes ok |
| tables | `runtime/gen/*` from the player's sdk/main: layout, 600 imports, 86 lifted SDK functions (NVN texture storage and the GPU kind chooser among them), 90 of 105 NVN device answers |
| lift | 55,277 functions, 5.1 M instructions -> 407 C files, 5 s |
| compile | all 409 files compile, 0 failures (about 12 min with 15 jobs on a loaded machine; 47 large functions fall back to lower optimisation) |
| link | `build/game.so` (154 MB) + `build/game_native` (4.4 MB) + `libssbu_glslang.so` |
| check | B2's lifter self-test (382 pair words, 25 SIMD words) and the B3 one (200 words, 4,000 cases) against Unicorn: 0 failures (needs `pip install unicorn`; `build/venv` is fine) |

Tools: `scripts/pkz.py` (list / check / extract `.pkz`; checked over all 72 archives), `scripts/pgdat.py` (DAT),
`scripts/nvn_procs.py` (NVN entry points of the sdk), `scripts/runtime_coverage.py` (imports vs handlers).

## How far it boots (stage 2, 2026-10-23)

```
cd build && mkdir -p run && cd run
SSBU_FOCUS_NOTIFY=0 SSBU_VK_MISSING=warn SSBU_ROMFS=$PWD/../../targets/romfs SSBU_USERDATA=$PWD/userdata \
    ../game_native ../../targets/exefs/main.elf 240
```

(argument 2 = seconds before the runtime stops it; the log goes to the console, the first calls of every import to
`boot_trace.txt`.) The process, its window and its audio stream are named **B3 Recomp** (app id `b3recomp`), so they do not
clash with B2recomp's `game_native`: `pgrep -x "B3 Recomp"` finds it.

- `SSBU_FOCUS_NOTIFY=0`: without it the applet model (nnmisc.c) **suspends the game whenever its window is not in the
  foreground** (mode 3, as on the console); a run on a desktop where the window loses the focus then looks hung.
- `SSBU_VK_MISSING=warn`: without it the Vulkan back end stops at the first operation it cannot do (see the blockers below);
  with it those are skipped and logged once each.
- Test aids used for the results below: `SSBU_SHOW=0` (window off screen), `SSBU_SHOT_EVERY=<n> SSBU_SHOT_DIR=<dir>` (a .bmp
  every n frames), `SSBU_SCRIPT="30:Z 36:Z 42:X ..."` (timed keys: Z/X = A/B, ! = Plus, u/d = up/down).

What runs now, measured on the developer's PC (RX 9070 XT, RADV), Vulkan back end:

1. Everything of stage 1 (image, `DT_INIT`, constructors, `nnMain`, RomFS, 72 `.pkz`, threads, NVN device / queue / pools).
2. **NVN texture setup**: storage size / alignment / class / sparse layout run the sdk's own NVN 13.3.2 code, found by name
   (section "NVN" of `docs/RE_NOTES.md`); self-test 22,396 cases, 0 failures (`SSBU_SDK_SELFTEST=1 RECOMPS_NVN_SELFTEST=1`).
   About 2,300 textures, 40,000 buffers, 7,900 shader programs (7,830 graphics, 74 compute) are set up.
3. Wwise starts (IOThread, BankManager, EventManager, Opus decoding threads), 23 guest threads in all.
4. **Frames are presented**: the loading emblem (frame ~15), then the **title screen** at about frame 600 (~20 s):
   "BAYONETTA 3", "Press any button", the copyright line and the game version **1.2.0**.
5. With scripted input: title -> **main menu** (Story / Options) -> difficulty -> Naive Angel Mode help and choice ->
   brightness -> auto-save notice -> **Story starts**: the opening movie `movie/ev0000.webm` cannot be played (no decoder on
   Linux, a failing placeholder) and is skipped; the game goes on into the prologue chapter "Phenomenal Affirmation":
   Plus opens the in-game **pause menu** (play time counting) and Photo Mode works.
6. 60 fps on the title and in the prologue (vsync); 240 s runs end on their timer, no crash.

**Nothing of the 3D world is visible**: in the prologue the screen is black behind the menus (57 draws per frame, about
5 % of draws skipped). All 2D UI (menus, text, help pictures, pause menu, photo mode) renders. No sound (see below).

## Next blockers (most urgent first)

1. **3D scene renders black.** Candidates, all logged: draws whose shader stage cannot be translated (skipped);
   `nvnCommandBufferDrawIndirect` not implemented in the Vulkan back end; compute dispatches that cannot be translated
   (skipped); `nvnCommandBufferSetDepthBounds`. The game makes heavy use of `MultiDrawElementsIndirectCount` (261,000
   calls in 240 s) and `DrawElementsIndirect`: whether those draw is the first thing to check.
2. **Maxwell shader translation** (`mx_translate.c`), stages not translated in a 170 s run: `SHFL` 1,335 (warp shuffle:
   GLSL `subgroupShuffle*`), `SUST` with an image handle in a register 147 (bindless images), `ATOM` / `ATOMS` / `RED` 43
   (global / shared atomics), `VMNMX` 9, `BAR` forms 9; 12 stages rejected by glslang (`textureOffset` with a non-constant
   offset: needs `textureGatherOffsets`-style lowering or the non-constant-offset extension).
3. **Movies**: `movie::` (subsdk0) plays VP9 WebM; Linux has only the failing placeholder (Media Foundation on Windows). The
   game skips a film it cannot open, so this does not block, but every cutscene is missing. Needs a VP9 decoder behind
   `movie::Extractor` / `Decoder` (libvpx or FFmpeg). B3's Extractor vtable differs from Smash's (SetDataSource at +0x78).
4. **Audio**: `nn::audio::OpenDefaultAudioOut` has no handler (Wwise output) -> silent.
5. **`nn::account::PopOpenUsers`** has no handler (called once at boot, harmless so far).
6. **The rest of the imports**: about 170 without a handler (libc stdio / dirent / time, libc++ streams); none reached yet
   beyond the two above.
7. **Compile time**: about 12 min for a full build (incremental relift: seconds).

## Known shortcuts (marked `shortcut:` in the code)

- `SdkMutexType`, `pthread_once`, `std::__call_once` wait by spinning on `Sleep(0)` (no kernel wait queue).
- `nn::fs` thread fs-context is a shared dummy object; `nn::oe` notifications / recording and `hid` mouse are no-ops.
- `scripts/pkz.py` decompresses Kraken entries only with an Oodle library the user supplies.
- The heap size (3,285 MB) is the console's documented application memory, not measured on hardware.
- NVN 55 additions (`nvn_gl.c`, `nvn55:` in the log): deferred binds are read when recorded; separate samplers are
  dropped and separate textures bound as combined handles; transform feedback, subroutines, checkpoints, subtile size and
  `MapVirtual` are logged no-ops.
- `nvnCommandBufferDispatchComputeIndirect` (Vulkan) reads the group counts from guest memory after a sync.
- `nn::os` ReaderWriterLock has no writer preference.
- The device fields the sdk's texture code reads are modelled (`nvn_sdk.c`); "compression supported" (+0x1b34 = 1) is
  inferred, its writer was not found.
- Not tested: Windows builds of these changes (only `TOOLCHAIN=linux` was built and run).
