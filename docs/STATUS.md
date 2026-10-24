# b3recomp status

Stage 5 (2026-10-24): the title, the cutscenes (VP9 movies through FFmpeg), the prologue and **the first fights** run, with
sound and scripted pad input (punch, kick, dodge, jump, movement: Verse 1 cleared, Verse 2 reached), without a "not
implemented" stop in 1,500 s of the prologue (chapter 1 reaches tessellation). 30 fps movies; **chapter-1 fights about 59 fps**
(were 44-52: a texture walk on every NVN flush, see Performance). Vulkan validation (core, sync, GPU-assisted): clean apart from benign / known items, listed below.

## What builds

`python scripts/setup.py --dump <dump>` runs all nine steps and they pass:

| step | result |
|---|---|
| deps, verify, convert, mods | the dump is the expected build (main build id `b47cc79b...`); main, sdk, subsdk0 -> ELF, segment hashes ok |
| tables | `runtime/gen/*` from the player's sdk/main: layout, 600 imports, 86 lifted SDK functions (NVN texture storage and the GPU kind chooser among them), 90 of 105 NVN device answers |
| lift | 55,277 functions, 5.1 M instructions -> 407 C files, 5 s |
| compile | all 409 files compile, 0 failures (about 12 min with 15 jobs on a loaded machine; 47 large functions fall back to lower optimisation) |
| link | `build/game.so` (154 MB) + `build/b3-game` (4.4 MB) + `libssbu_glslang.so` |
| check | B2's lifter self-test (382 pair words, 25 SIMD words) and the B3 one (200 words, 4,000 cases) against Unicorn: 0 failures (needs `pip install unicorn`; `build/venv` is fine) |

Tools: `scripts/pkz.py` (list / check / extract `.pkz`; checked over all 72 archives), `scripts/pgdat.py` (DAT),
`scripts/nvn_procs.py` (NVN entry points of the sdk), `scripts/runtime_coverage.py` (imports vs handlers).

## How far it boots (stage 2, 2026-10-23)

```
cd build && mkdir -p run && cd run
SSBU_FOCUS_NOTIFY=0 SSBU_VK_MISSING=warn SSBU_ROMFS=$PWD/../../targets/romfs SSBU_USERDATA=$PWD/userdata \
    ../b3-game ../../targets/exefs/main.elf 240
```

(argument 2 = seconds before the runtime stops it; the log goes to the console, the first calls of every import to
`boot_trace.txt`.) The process, its window and its audio stream are named **B3 Recomp** (app id `b3recomp`), so they do not
clash with B2recomp: `pgrep -x "B3 Recomp"` finds it. The executable is `build/b3-game` (`b3-game.exe` on Windows; it was
`game_native`, the same name as B2recomp's, until 2026-10-24).

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

## Stage 4 (2026-10-24): movies, the prologue

- **Movies on Linux**: `nnmovie_mf.c` keeps the movie:: protocol (Extractor / Decoder vtables, buffers, events) and uses
  FFmpeg underneath on Linux (libavformat on the bytes the game's reader returns, libavcodec per selected track, swscale to
  the tight NV12 the game uploads, swresample to s16 for audio tracks); Windows keeps Media Foundation (only its calls moved
  into `src_read` / `src_seek` / `src_close`; **the Windows build was not rebuilt**). B3's movies are VP9 1600x900 30 fps
  without an audio track (the soundtrack comes through Wwise).
- Two things kept them from playing: `nn::os::TryWaitEvent` had no handler (B3's player polls its decoder events with it:
  87,778 stub calls), and B3 allocates the Extractor / Decoder objects with its heap-aware operator new, so their deleting
  destructors must call the game's `operator delete` (free() refused the block: a guest trap at the end of the first movie).
- A 480 s run from the save (A every 3 s): `ev0000` (163 s, with the game's subtitles), `ev0005`, `ev0010`, `ev0011` play;
  the prologue goes on through in-engine scenes (the hexagon city, Bayonetta and Viola, an arena with lava, close-ups,
  "You have a job to do, soldier!"). The summoned-demon sequence of stage 3 no longer holds it (it was waiting for the
  opening movie). 18,149 frames, 5 M draws, 0 skipped.
- **Frame rate**: 60 fps in-engine; 30 fps while a movie plays (the movies' rate) and in one heavy scene (983 draws). Indirect
  records in a compute buffer cost a GPU wait only after new GPU writes (was ~150 waits a frame: 27-49 fps).
- **Vulkan validation**: not run at the time (the layer was installed later: see the next section).

## Vulkan validation (2026-10-24, layer 1.4.363, RADV on an RX 9070 XT)

`SSBU_VK_VALIDATE=1 VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation`; the messages are counted per VUID and the sorted totals
printed at exit (`vk-validation summary:`). Raise the layer's cap of 10 per ID with
`VK_KHRONOS_VALIDATION_DUPLICATE_MESSAGE_LIMIT`. Each pass ran from the save through the title, the opening movie and the
prologue (300-420 s, 9,000-12,600 frames, A every 3 s):

| pass | settings | before | after |
|---|---|---|---|
| core | default | 1 leak at `vkDestroyDevice` (14 objects) + benign | benign only |
| sync | `VK_KHRONOS_VALIDATION_VALIDATE_SYNC=true` | `SYNC-HAZARD-WRITE-AFTER-WRITE` (vkCmdBlitImage twice into one image) | benign only (10,812 frames) |
| GPU-AV | `VK_KHRONOS_VALIDATION_GPUAV_ENABLE=true` | `SharedMemoryDataRace-*` in 2 compute programs | the same 2 (known, below) |

Fixed:
- The loading bar / first-run screen blitted over the presented frame (and the bar over its track) with no barrier
  between the transfer writes: `xfer_wait()` before each.
- Shutdown now frees the compute buffers' landing buffers, the read-back free list, the kept vertex buffers and the
  occlusion query pool (the 14 leaked objects).
- Translator: an 8 / 16-bit `STS` was a read-modify-write of the 32-bit word, so lanes storing neighbouring bytes in
  the same instruction lost all but one; now `atomicAnd` + `atomicOr`. `MX_VK_VERSION` 10 (all stages are translated
  again on the next run: one warm-up).
- Under validation, shader modules carry the name `program #<n> stage <s>` (GPU-AV prints it), and
  `nvn_vk: shader module 0x... = ...` is logged for each.

No message about the paths added in stages 3-4 (storage images, depth bounds, passes without attachments, indirect
draws read on the CPU, the movie's NV12 uploads).

Left, judged benign or known:
- `Undefined-Value-ShaderOutputNotConsumed-DynamicRendering` (warning, ~400,000 per run): a fragment stage writes a
  colour output and the pass has no attachment there (depth-only and target-less passes). Vulkan discards it.
- `SharedMemoryDataRace-RaceOnStore` in program #7836 (compute, 64 wide): every invocation stores 0 to `smem[0]` at
  entry, the same value from the same instruction. The game's own shader does it the same way; harmless.
- `SharedMemoryDataRace-RaceOnLoadStoreVsAtomic` in program #7839: init by invocation 0, `barrier()`, `atomicMin/Max`,
  `barrier()`, loads. Correct as written, but the translator puts the barriers inside its `for (;;) switch (pc)` dispatch
  loop, so invocations reach the same barrier in different loop iterations. Per SPIR-V that is not uniform control
  flow, so GPU-AV cannot pair the barriers. It works on RADV, where a 64-wide group is one wave. A structured control
  flow for stages with `BAR` would fix it (not done).
- `WARNING-Setting-Limit-Adjusted`: GPU-AV itself.

## Stage 5 (2026-10-24): the prologue goes on into chapter 1's cutscenes

- **The prologue stopped on a still of Viola** (about 640 s from the save, frame ~19,000): `ev0011` is opened and
  pre-rolled while `ev0010` plays, then taken in one burst when `ev0010` ends. B3's player [game 0x7100cd6cb4] keeps
  decoded frames in a pool of 16 nodes. A frame acquired while the pool is empty [0x7100cd7244] is neither queued nor
  released, so it is lost and its output buffer leaks. This decoder has no decoding time and offered 48 output buffers
  (SSBU's number), so the burst overran the pool, the picture stopped after 16 frames and the decoder had no free buffer
  left. **16 video output buffers** now (the codec stalls before the pool runs out). The earlier movies were losing frames
  the same way (300 frames per 18.7 s of movie time before, 300 per 10 s now).
- A 900 s run from the save (A every 3 s): `ev0000`, `ev0005`, `ev0010`, **`ev0011`, `ev0020`, `ev0021`, `ev0022`**
  play, through the in-engine prologue scenes and into chapter 1's New York opening (the streets, the pizza truck).
  31,336 frames, 5.4 M draws, 0 skipped, no stop.
- Linux: `libopus-0.dll` maps to `libopus.so.0` (`nn::codec`'s HardwareOpusDecoder reported a work buffer of 0 bytes;
  Wwise does its own Opus, so this was not the stall).
- Test aids: `kill -USR1 <pid>` (Linux) prints each thread's guest thread name, current guest function, LR and x29 frame
  chain (the hang above was found with it; ptrace is not allowed here). `SSBU_MOVIE_LOG` lines now name the decoder and
  the guest caller.
- `SSBU_SKIP_MOVIES=1` takes another path: the A presses reach the menu and open **Chapter Select, "Prologue: A Chaotic
  Encounter"**. After the (cut) movies `ev0020`..`ev0026` it holds one camera view of water and buildings at 563 draws a
  frame for over 4 minutes (not looked into; cutting movies short may itself be the cause).

### Gameplay (2026-10-24)

- **The first fights run.** Run `long1` (1,500 s, A every 3 s) took the Chapter Select route: Prologue "A Chaotic
  Encounter", movies `ev0020` .. `ev0026` at full length, then gameplay at ~760 s (frame ~23,000) with the combat HUD
  (Verse 1, the Punch / Kick / Shoot / Jump / Dodge prompts, combo points) against Homunculi and a giant Homunculus.
  With A alone Bayonetta keeps dying ("THE WITCH HUNTS ARE OVER", retry) in Verse 1.
- **Input beyond A works.** Run `fight1` (1,500 s, the same A presses plus an `SSBU_SCRIPT` of 1,716 steps from 700 s:
  punch `V`=Y, kick `C`=X, dodge `S`=ZR, jump `X`=B, left stick right / left, punch+dodge): Verse 1 is cleared (1,525
  combo points x1.75 on one shot) and the fight goes on into **Verse 2** (another giant Homunculus, another arena).
  52,448 frames, 32 M draws, 0 skipped, no stop. `SCRIPT_MAX` is 4,096 steps now (was 256).
- Screenshots: `/tmp/shots/b3_first_fight_verse1.png`, `b3_verse1_start.png`, `b3_verse1_combo.png`, `b3_verse2.png`,
  `b3_verse2_fight.png`.

## Performance (2026-10-24): chapter-1 fights at 60 fps

**Benchmark**: the save as it is now (the user's chapter-1 progress) → Chapter Select → chapter 1 → Verse 1 on the grass
field (from about frame 19,400), A every 3 s plus the fight script of stage 5 from 560 s, 1,100 s, `SSBU_AUDIO_MUTE=1`,
`SSBU_VK_MISSING=warn` (the chapter's tessellated draws are skipped: blocker 3). Numbers over the 300-frame windows from
frame 19,800 to the end of the run (`nvn_vk:` / `nvn_perf:` / `nvn_vk_timeline:` lines). The runs use a copy of the save
(`/tmp/b3_userdata_<run>`): the game autosaves.

| run | build | fps avg (min) | 1 %-low fps | frame time | render thread a frame (batch start → last submission) |
|---|---|---|---|---|---|
| base1 | before | 44.5 (30.5) | 31.1 | 23.3 ms | - |
| base2 | before | 49.4 (35.7) | 32.5 | 20.6 ms | 15.1 ms |
| base3 | before (profiled 90 s) | 52.1 (43.1) | 35.0 | 19.3 ms | 13.1 ms |
| fix2 | flush index | **59.3 (50.6)** | **47.4** | **16.9 ms** | 12.4 ms |
| o2a | flush index, host -O2 | 59.4 (51.9) | 49.3 | 16.8 ms | 11.9 ms |

(Run `fix1`, the same build as `fix2`, ran 17-24 fps from 640 to 810 s with every thread slowed alike, sampler included, and 59.2 fps
from frame 22,800 to the end: taken as outside CPU load and left out.)

- **The cause**: `SSBU_LPROF` (below) found **83-96 % of the busy time of the game's MainDraw and Flush threads in
  `nvn_gl_memory_written`**: every `nvnMemoryPoolFlushMappedRange` / `nvnBufferFlushMappedRange` walked every texture ever
  made (the list is never unlinked) to mark the ones it overlaps; B3 flushes many ranges a frame. Now a span index of the
  live textures sorted by address (binary search, running maximum end), rebuilt when a texture is made or finalized;
  `SSBU_TEXSPAN_CHECK=1` compares every lookup with the walk (240 s, no difference). After it, MainDraw is 12 % busy.
- **Tried, not kept**: (1) the kept vertex buffers of finding 140 (`vrc_*`) for streams no compute buffer overlaps (they are
  off whenever any compute buffer is live, i.e. always in a B3 fight): the ring copies fell from 29 to 7 MB a frame, but the
  render thread per draw did not move (13.2 against 13.6 us: the write-watch questions cost what the copies saved), so the
  change was dropped. (2) The host sources at -O2 (`--host-opt=O2`): 13.8 against 13.6 us per draw, the same within the
  noise; the default stays -O1.
- **What is left on the render thread** (about 60 % busy in a fight): memcpy (46 %: compute buffers filled from guest
  memory at a dispatch, texture uploads, the copy-back of compute results), `cb_keep_back`, `draw_prepare`. The dips
  (min 50-52 fps windows) were not looked into one by one.
- **`SSBU_LPROF=<start s>,<seconds>[,<file>]`** (Linux, test only, `runtime/lprof.c`): every thread is sampled with a
  real-time signal (SDL3's threads block SIGPROF), PC + host call chain; `scripts/lprof_report.py <file>` lists the busy
  threads (innermost frame not waiting in libc) with their self / inclusive functions (nm of b3-game, game.so: `f_<guest
  address>`). Lifted code has no unwind tables: its chains stop at the innermost lifted function.

## Next blockers (most urgent first)

1. **Camera (right stick) on a real pad**: the user's chapter-1 play by hand: the right stick does not turn the camera.
   Found so far: B3 reads `GetNpadState(NpadFullKeyState)` and converts all four stick axes [game 0x7100cc53d0]; the SDL
   mapping of the user's "Steam Controller" (28de:1304) has `rightx:a2, righty:a3`; the pad self-test passes (49/49). Not yet
   confirmed in a game run (the work is paused: `git stash` "camera right-stick WIP (paused)": script keys 4/8/6/2 for the
   right stick, `SSBU_HIDLOG` callers of the state reads, and the Linux path of `config/controllers.ini`, which is never
   written on Linux now (the path is cut at `\`, so it points into `build/b3-game/..`)).
2. **Which route a run takes** depends on the save and on when the A presses land on the title (movie route, Chapter
   Select → Prologue, Chapter Select → chapter 1). An `SSBU_SEQ` with `@draws` anchors would make it reproducible.
3. **Tessellation** (29 programs): reached in chapter 1 (NVN primitive type 14 = patches; the run stops there without
   `SSBU_VK_MISSING=warn`). Translator stages and pipeline support are missing.
4. **Fight dips**: 50-52 fps windows and 1 %-lows of 47-49 fps remain (render thread memcpy and compute-buffer handling).
5. **Picture correctness**: no reference images to compare with.
6. **Windows build** of the movie changes and of the b3-game rename (not compiled here).
7. **Barriers in the translator's dispatch loop** (`SharedMemoryDataRace-RaceOnLoadStoreVsAtomic`, program #7839):
   structured control flow for stages with `BAR`.
8. **First-run warm-up**: minutes of held loading screens after a translator change.
9. The static scene after `SSBU_SKIP_MOVIES` (stage 5), `nn::account::PopOpenUsers` (called once, harmless), the
   `nn::audio` Resampler imports (not reached), about 170 other imports without a handler (none reached).

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
- Movies (FFmpeg): packets of a selected track read ahead of time wait in a queue of 512; past that the oldest is dropped
  (`shortcut:` in `nnmovie_mf.c`).
- Indirect draws are read on the CPU (one Vulkan draw per record) instead of `vkCmdDrawIndirect*`; records in a compute
  buffer of the batch cost a submit and a wait.
- Storage writes of a vertex / fragment stage are made visible at the end of the pass (a barrier), not between draws of the
  same pass (as on NVN without an explicit barrier).
- Not tested: Windows builds of these changes (only `TOOLCHAIN=linux` was built and run).
