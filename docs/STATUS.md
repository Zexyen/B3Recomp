# b3recomp status

Stage 3 (2026-10-24): the title screen and the start of chapter 1 render with their 3D scenes, at 60 fps, with sound and
without any "not implemented" stop in a 300 s run. Cutscenes (movies) are still skipped.
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

## Stage 3 (2026-10-23): the 3D world, shader translation, Vulkan gaps

Stage 3 was cut short: the PC is in use, so after the first runs below **no game run was allowed**. Everything after
those runs was checked offline only (builds, the translator check below). What a run showed before the stop:

- **The Vulkan back end skipped every indirect draw** (`DrawArraysIndirect`, `DrawElementsIndirect`, `MultiDraw...Indirect`,
  `...IndirectCount`, 261,000 calls in 240 s): it had no case for the front end's `OP_DRAW_INDIRECT`. Implemented (one Vulkan
  draw per record, the records read on the CPU once compute output there has landed). Two 60 s runs and one 150 s run
  after that: no crash, the "DrawIndirect not implemented" report gone.
- **"3D black" in chapter 1 is a loading screen that never ends.** Zoomed in, the faint dot bottom right is the loading
  emblem, dimmed: after Story (or Continue) the game stays on that screen; the pause menu works during it, which is why
  stage 2 took it for gameplay. Thread samples (gdb) during it: no deadlock; the main thread presents and waits on GPU
  syncs, the loader and file threads are idle. What it waits for is not known yet.
- One frame's pass list (`SSBU_VK_DUMPTEX=<frame>,1x1,0`): about 65 draws, a 17-draw depth pass, a 16-draw shadow map, a
  bloom chain, no scene pass: consistent with GPU-driven drawing whose culling compute (atomics) did not run.
- **Audio**: Wwise opened its output with SDK 13's event-less `OpenDefaultAudioOut(AudioOut*, const AudioOutParameter&)`,
  which had no handler. Added; a run confirmed sound (48 kHz stereo on PipeWire, buffers appended and released). Whether this
  unblocks the chapter loading was not observed (the run was interrupted).
- One 150 s run presented only 518 frames (about 3.5 fps), two later 60 s runs 2,300+ frames (with and without the audio
  output): the slow run is not explained.

Offline, `tools/mxcheck.c` translates every stage a run dumped and compiles it with glslang (the same path as the Vulkan
back end):

```
SSBU_SHADERDUMP_ALL=<dir> ...game run...                       # once: s<program>_<index>.mxd, the game's code: keep out of the repo
clang -O1 -I runtime tools/mxcheck.c runtime/mx_translate.c -lglslang -lglslang-default-resource-limits -lm -o build/mxcheck
build/mxcheck <dir> [-v] [-o <dir for the GLSL of failing stages>]
```

On the 7,838 vertex / fragment / compute stages of a run: **5,627 compiled at the start of stage 3, 7,838 now**. Added:
`SHFL` (warp shuffle on `subgroupShuffle`, 32-lane warps inside 32/64-wide subgroups), `VOTE` / lane id / lane masks on KHR
subgroups, `FSWZADD` (with `.NDV`), `BAR.SYNC`, `ATOM` / `RED` / `ATOMS` (global through per-op helpers on the storage
blocks, CAS loops where GLSL has no atomic), image accesses whose handle register was loaded from an image slot (all 799 of
them), texel offsets from registers (lowered), `VMNMX`, `SAM` / `RAM`. Not covered: the 29 programs with **tessellation**
stages (the translator has no tessellation or geometry stage; their draws are reported as missing).

Vulkan back end, built but not run: **storage images** (417 fragment and 4 compute stages store to images; the back end
ignored `nvnCommandBufferBindImage`): handles per stage, STORAGE usage on colour images whose format allows it, views, the
descriptor layouts (per pipeline, universal, compute), GENERAL layout, barriers after a storing pass or dispatch. **Depth
bounds test** as dynamic state.

### Game runs of 2026-10-24 (the list above, validated)

Run: from the save, `SSBU_PRESS=Z,25,3` (A every 3 s), Vulkan back end, no `SSBU_VK_MISSING`. What renders now (pictures of
the developer's run: title, Bayonetta falling through the sky at the start of chapter 1, a summoned demon sequence):

1. **The chapter-1 load finishes.** The "endless loading screen" of stage 2 was the scene rendering every lit pixel as
   Inf: the lighting divides by an exposure value that three **draws without render targets** compute (a luminance
   reduction over 90x51 / 90x1 whose fragment shaders only store to buffers). The back end skipped such draws and, for the
   graphics stages, also bound stored-to buffers as stream-ring copies (the writes were lost). Both fixed; found with the
   pass dump (`SSBU_VK_DUMPTEX`), the draw log (`SSBU_VK_DRAWLOG`, now with storage ranges and guest-memory writes) and a
   write watch on the exposure buffer (`SSBU_VK_WATCHFS`, test only).
2. **Frame rate**: 60 fps (vsync) on the title and in chapter 1 (16,841 frames in 300 s). The 3.5 fps case had two causes:
   (a) compute output is copied back to guest memory every frame (about 16 MB) and the compute buffers were in device-local
   host-visible memory (BAR), which the CPU reads at a few tens of MB/s: 300 ms a frame; now host-cached memory (4 ms);
   (b) after a translator version change (`MX_VK_VERSION`) every stage is translated and compiled again while the loading
   screens are held for the workers: the first run after such a change takes minutes to reach the title (one-time).
3. **Indirect draws**: the culled scene draws (96 draws in the depth pass where there were 17); indirect records written
   by compute are read through the compute buffer's mapping (no landing of every buffer).
4. **Storage images**: exercised (fragment program #1693 stores to an image every frame), no stop, no crash. Not checked by
   the Vulkan validation layer: it is not installed on this PC (`vulkan-validation-layers`).
5. **Depth bounds**: used by the game (stage 2 stopped on it), runs now; not checked picture by picture.
6. **Title background and glow**: the backdrop is the 3D demon of the title; the "Press any button" glow is no longer
   blown out (both were the Inf exposure).
7. **No "not implemented" stop** in 300 s without `SSBU_VK_MISSING=warn`; 0 skipped draws.
8. Audio: Wwise output plays (stage 3, first half).

Still open from the list: the translated instructions are only checked by these pictures (no reference images of the
console), the texel-offset lowering's implicit-LOD shortcut, `SAM` / `RAM` as no-ops.

## Next blockers (most urgent first)

1. **Past the opening**: after the fall, chapter 1 shows a long animated sequence of a summoned demon (about frame 10,000
   on); A presses every 3 s do not end it. It may want another input (a QTE prompt is not visible), or the opening movie
   it follows (`ev0000.webm`, skipped) drives it. Next: script other buttons / sticks, watch the game's input reads.
2. **Movies**: `movie::` (subsdk0) plays VP9 WebM; Linux has only the failing placeholder (Media Foundation on Windows), so
   every cutscene is skipped. Needs a VP9 decoder behind `movie::Extractor` / `Decoder` (libvpx or FFmpeg). B3's Extractor
   vtable differs from Smash's (SetDataSource at +0x78).
3. **Picture correctness**: no reference to compare with; the Vulkan validation layer is not installed here (would check
   the new storage-image, depth-bounds and empty-pass paths).
4. **Tessellation stages** (29 programs): translator and pipeline support; not reached in 300 s.
5. **First-run warm-up**: minutes of held loading screens after a translator change (all 7,900 programs again).
6. `nn::account::PopOpenUsers` (called once, harmless), the `nn::audio` Resampler imports (not reached), about 170 other
   imports without a handler (none reached).

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
- Translator (stage 3): `SAM` / `RAM` are no-ops (helper invocations assumed active for quad shuffles); a register texel
  offset of an implicit-LOD sample is scaled by level 0's size; storage images need a format with storage support and the
  same texel size as the shader's typed format (else logged as missing).
- Indirect draws are read on the CPU (one Vulkan draw per record) instead of `vkCmdDrawIndirect*`; records in a compute
  buffer of the batch cost a submit and a wait.
- Storage writes of a vertex / fragment stage are made visible at the end of the pass (a barrier), not between draws of the
  same pass (as on NVN without an explicit barrier).
- Not tested: Windows builds of these changes (only `TOOLCHAIN=linux` was built and run).
