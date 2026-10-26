# b3recomp status

Stage 5 (2026-10-24): the title, the cutscenes (VP9 movies through FFmpeg), the prologue and **the first fights** run, with
sound and scripted pad input (punch, kick, dodge, jump, movement: Verse 1 cleared, Verse 2 reached), without a "not
implemented" stop in 1,500 s of the prologue and 1,100 s of chapter 1 (tessellation now drawn, no SSBU_VK_MISSING=warn). 30 fps movies; **chapter-1 fights about 59 fps**
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

Tools: `scripts/pkz.py` (list / check / extract `.pkz`; checked over all 72 archives; Kraken entries from a
`--pkz-dump` dump, section "RomFS archives"), `scripts/pgdat.py` (DAT),
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
them), texel offsets from registers (lowered), `VMNMX`, `SAM` / `RAM`. The 29 programs with **tessellation** stages followed
later (see "Tessellation", 2026-10-24); geometry stages are still not covered (none in the dump).

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
  `V`=Y, `C`=X, dodge `S`=ZR, jump `X`=B (the game's controller page shows X = punch, Y = shoot, A = kick; the labels
  first written here had them swapped), left stick right / left, punch+dodge): Verse 1 is cleared (1,525
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

## Tessellation (2026-10-24)

B3 has 29 tessellation programs, each a separate NVN program per stage (bound by stage mask): 29 control and 29
evaluation stages. All are the same kind: the GLSLC tessellation info of the control section (+0x738 primitive mode,
+0x73c spacing, +0x740 point mode, +0x744 vertex order, OpenGL enums) says GL_TRIANGLES, GL_EQUAL, no points, GL_CCW;
the control stages' program headers say 3 output vertices and 6 patch attributes (the levels).

- **Translator** (`mx_translate.c`, after suyu's recompiler): ALD / AST with the vertex register (bits 39..46) on input
  arrays, the control stage's outputs at `[gl_InvocationID]` (ALD.O reads them back; in the evaluation stage .O reads its
  inputs), patch attributes 0..3 / 4..5 = outer / inner levels, `gl_TessCoord` (0x2f0 / 0x2f4), invocation info =
  `gl_PatchVerticesIn << 16`, `ISBERD` (a move), `VMAD`, `VSETP`; `layout(vertices = N)` and `layout(triangles,
  equal_spacing, ccw)`; the built-in blocks redeclared to match the neighbours. Generic patch attributes fail (none used).
  `tools/mxcheck`: **7,896 / 7,896 stages** translate and compile (the 58 tessellation stages included); the other 7,838 give
  byte-identical text (no `MX_VK_VERSION` change; the tessellation stages have a hash salt of their own).
- **Vulkan**: NVN primitive 14 with both stages: a whole pipeline (no libraries) per key + stages + patch size, made at
  the first draw (`tess_pipe_get`, not in pipes.bin or the workers), `PATCH_LIST`, `patchControlPoints` = the NVN patch
  size (`OP_PATCHSIZE`, ignored before), domain origin `LOWER_LEFT` (the viewport is flipped the OpenGL way);
  `draw_prepare` binds samplers, images, buffers and descriptors for the stages {0, 1, 3, 4}; forced outputs chain
  fragment <- evaluation <- control <- vertex. The glslang shim takes stages 3 and 4.
- **Runs** (chapter 1 by Chapter Select, the fight script, muted, **without `SSBU_VK_MISSING=warn`**): 1,100 s, 46,666
  frames, **0 skipped draws** (the earlier runs with `warn` skipped 4,552-5,585), no stop. One tessellated pipeline (programs
  #3945 / #3950 / #3951 / #3946, patch size 3), about 2,700 tessellated draws, one a frame in runs from frame ~17,600.
  The fight: 59.3 fps (min window 55.5, 1 %-low 48.7), as before.
- **Validation** (core, and core + sync, over the same route): only the known
  `Undefined-Value-ShaderOutputNotConsumed-DynamicRendering` warning; the first run found
  `VUID-RuntimeSpirv-OpVariable-08746` (built-in block mismatch vertex -> control), fixed.
- Test aids: `nvn_vk: tessellated pipeline <n> at frame ...` and `tessellated draws in frames a..b: n` lines, the totals at
  exit; `SSBU_VK_TESS_ALT=1` leaves the tessellated draws out on odd frames, `=2` draws them magenta there (with
  `SSBU_SHOT_BURST=<period>,2` for pairs of pictures).

## Magenta textures: invalid ASTC blocks (2026-10-24)

The user's hand run showed bright magenta on clothes (Luka's shirt), Bayonetta's guns, heels and accessories, the purple
light pillar and the Verse results' ink splashes. The GPU here (RADV, RX 9070 XT) has no ASTC, so `tex_decode.c` decodes B3's
ASTC textures on the CPU. It gave every **invalid** block the specification's error colour, opaque magenta. B3's chapter 1
has about 105,000 of them (`SSBU_ASTC_LOG=1`): **all-zero blocks** (a reserved block mode), in ASTC 6x6 textures that
are bound in many places (the same counts, image after image: 7 % of the 1024x1024 level, whole 32x32 and 2x2 levels).
Tegra returns transparent black for them (yuzu PR #8910 made its decoders do the same), so nothing shows on the console.

- **Invalid blocks are transparent black now.** Same route and scene, before / after: `/tmp/shots/astc_before_27000.png`
  (Verse 1 results: magenta ink, shirt, guns), `/tmp/shots/astc_after_25500.png` (results: white / black ink),
  `/tmp/shots/astc_after_chars.png` (Bayonetta close and in the fight: gold trim, no magenta). Chapter 1, 1,100 s without
  `SSBU_VK_MISSING=warn`: no stop, 0 skipped draws, fight 59.7 fps (min window 56.5).
- Found on the way: **HDR colour endpoint modes** (2, 3, 7, 11, 14, 15) also became magenta; Tegra decodes them in these
  formats (full profile). Now decoded (LNS -> FP16, clamped into the 8-bit images, per partition and channel). B3's
  chapter 1 has none (the counter says 0), so this did not cause what the user saw.
- `tools/astccmp.cpp` compares the decoder with ARM's astc-encoder (fetched separately) on random blocks of all 14
  footprints: 72,524 valid blocks (22,840 with HDR endpoints), none more than 1 apart (the decoder before: 22,828 apart).
  It skips blocks either side rejects, so it does not test what an invalid block becomes.
- The exit log has `tex_decode: ASTC blocks decoded on the CPU with HDR endpoints: N; invalid (transparent black): M`;
  `SSBU_ASTC_LOG=1` (test) names each image with invalid blocks (format, size, count, the first non-zero one).
- **Correction (2026-10-25):** most of those all-zero blocks were not in the game's data; they came from the layout bug
  below (the padding GOBs of sheared textures). Transparent black stays the right answer for the real ones.

## Block width of packaged textures: medal-screen flicker, gun texture (2026-10-25)

The user's hand run of ded5dc9 showed the fire/ink panel of the Verse medal screen flickering, and Bayonetta's guns with
blue bands and patchy gold. Both came from one bug: **byte 1 of the packaged texture layout is the block width** (log2
GOBs) and `tex_layout` ignored it. B3's ASTC 6x6 effect atlases and Bayonetta's gun texture have byte 1 = 4 (layout
`03 04 00 00 07 00 01 00`): a block of 16 x 8 GOBs, one 64 KB tile.

- **The rule, read off the game's memory** (`SSBU_TEX_RAWDUMP`): a level at least one block wide has its rows padded to a
  multiple of the block width; a narrower level is not padded at all. 1024x1024 ASTC 6x6: rows of 43 -> 48 GOBs, 22 -> 32,
  then 11, 6, 3, ... as they are; levels at 0, 589,824, 851,968, 897,024, 909,312, ... (every level of the dumps now
  decodes with 0 invalid blocks). The layer stride counts the block width too. `tools/astccmp` checks these offsets on
  every run.
- **Flicker**, same scripted route, the medal screen (214 frames, right half of the picture, `/tmp/re/gaptest.py`): with
  byte 1 ignored, 205 frames change and change back (A-B-A); now 14. The change over 1 frame fell from 7.7 to 3.7 (over 8
  frames: 10.7 and 9.4), so what moves now is the ink's animation. Pictures: `/tmp/shots/flicker_before_after.png` (six
  frames each, top before), `/tmp/shots/flick5_seq.png`.
- **Invalid ASTC blocks** in a 400 s run: 92,237 with byte 1 ignored, 2,083 now. Those are all in one building normal map
  (ASTC 10x10, 1024x1024, plain layout) whose unused areas are all-zero blocks; astc-encoder rejects them too.
- **Guns**: their colour texture is ASTC 6x6 sRGB 1024x1024 with byte 1 = 4. Before: sheared bands, 2,183 invalid
  blocks; now clean: `/tmp/shots/gun_albedo_old_vs_new.png`. The texture itself is a saturated purple body with gold
  filigree and a gold-ringed disc, so the guns stay purple and gold. In the game (Tokyo boss cutscene):
  `/tmp/shots/guns2_9936_crop.png`, solid purple and gold, no bands. Not reproduced here: the user's Verse 1 close-up
  (another save point); a look by hand there is the check left.
- **The decoder on real data**: all 3,010 ASTC images of a 380 s run (`SSBU_ASTC_DUMP`, 11 formats, 4x4 to 10x10, linear
  and sRGB) compared block by block with astc-encoder (`astccmp --file`): none more than 1 apart, and every block this
  decoder rejects the reference rejects too.
- **Speed**: the same scripted 700 s run on this save (it ends in the heavy Tokyo scenes, not the chapter-1 fight) from
  frame 19800: 44.1 fps before, 35.0 and 42.8 in two runs after. The difference is a few windows with stalls of about
  160 ms (p99), at different frames in each run and with no texture uploads in them; the median frame time is the
  same. The chapter-1 fight (59.7 fps) needs the older save and was not run again.

## Compute buffers: a full table (2026-10-25)

The user's later save (Tokyo chapter) stopped with "more than 256 storage buffers written by compute in one batch".
A full table now lands its least recently used kept buffer and reuses the slot (72 times in 700 s there), and the table
holds 1,024. The heavy Tokyo scenes ran at 35-44 fps then; see the next section.

## Performance in Tokyo: Shibuya and the heavy Tokyo scenes (2026-10-25)

**Benchmarks** (`SSBU_AUDIO_MUTE=1`, each run on a copy of a save; per-frame times from `SSBU_FRAMETIMES`, the
statistics over the frames the game paces at 60 = present interval 1):
- *Shibuya*: the user's save (Continue → Tokyo: Shibuya street fight), the stage-5 fight pattern from 40 s to 330 s,
  frames 1500 to the end.
- *Tokyo (old route)*: a copy of the earlier save (`bench_bw`'s), the stage-5 script from 560 s, 700 s, frames 19800 to
  the end: the heavy scenes measured at 35-44 fps before.

| | avg fps | 1 %-low | p50 / p90 / p99 ms | frames over 20 / 50 / 100 ms | frames at 60 |
|---|---|---|---|---|---|
| Shibuya, before | 57.4 | 38.8 | 16.8 / 20.6 / 25.8 | 1814 / 12 / 1 | 82.5 % |
| Shibuya, after | **59.9** | **51.7** | 16.7 / 17.6 / 19.3 | 146 / 4 / 0 | **98.0 %** |
| Tokyo, the changes switched off | 51.8 | 40.2 | 18.5 / 21.5 / 24.8 | 3667 / 64 / 59 | 49.8 % |
| Tokyo, after (without the bit split) | 59.7 | 53.9 | 16.7 / 17.6 / 18.6 | 29 / 13 / 8 | 98.8 % |
| Tokyo, after (all) | **59.5** | **52.9** | 16.7 / 17.6 / 18.9 | 98 / 15 / **0** | **98.4 %** |

("Switched off": `SSBU_VK_VRC=0`, `SSBU_TEXDEC_THREADS=0` and an always-sync test switch that was not committed; the live
list stays. Runs of the same build differ by about ±1 % at 60 and in where the trains of long frames come; another
Tokyo run of the last build: 59.8 fps, 98.5 % at 60, one frame over 100 ms (104, below).)

What it was, in order of the commits:
- **The game's 30 fps is the game's.** Its main loop picks a frame-rate mode each frame (`7100670b80` → `7100d02de0`,
  applied at `7100cfd9b8` from the table 1 / 2 / 4) from its own state flags: interval 2 on the title, in menus and in
  cutscenes (flags `50000208`, `50000008`). The user's 30.0 fps stretch (frames 5100-11500 of their log) is such a span.
  Not changed; letting those run at 60 would change the game (its logic steps by the mode).
- **Kept vertex buffers while compute is live** (`vrc_cb_touch`): every vertex stream went through the stream ring because
  B3 dispatches compute every frame: 64 MB copied a frame, the render thread's largest cost. Now 5.6 MB. (The
  Performance section above dropped the same idea in chapter 1, where the write-watch questions cost what the copies
  saved; in Tokyo the copies are three times larger and it pays.)
- **ASTC decode on worker threads** (`decode_par`): the 45-170 ms stalls of Shibuya were new 1024x1024 ASTC textures
  decoded on the render thread (15-20 ms each, several at once when the game streams an area in). A 1024x1024 texture now
  takes 5.3 ms. `SSBU_TEXDEC_CHECK=1` compared 1,301 parallel jobs with one thread's decode: no difference.
- **Live compute buffers in a list** for `buf_ref`'s searches (the walk over the table was 17 % of the render thread).
- **Buffer copy / update / clear wait for compute only when they overlap it** (`cb_sync_needed`): each of these CPU-side
  commands waited for the GPU and landed every live compute buffer; B3 issues a clear and four 32-byte copies a frame, so
  every frame paid a GPU wait and 4-24 MB of copies. The render thread in Shibuya: 10.4 → 3.9 ms a frame.
- **The 150-170 ms trains in Tokyo** (the "160 ms stalls"; 59 frames over 100 ms in a 700 s run, about every 6 frames
  while a train lasts): the main thread in the game's `f_7100b382c0`, called from a deep recursion of `f_71007e1fe4` (a
  tree walk): it splits a 32-bit value into its even and odd bits with three bit reversals and a 32-step loop, 171.6 ns a
  call lifted. New: `lifter/recomp_eh.py` `HOST_OVERRIDES` sends every direct call of a chosen game function to a host
  function (`runtime/game_shim_eh.c`); `h_morton_split32` takes 1.7 ns. `SSBU_HOSTFN_CHECK=1` (test): 200,000 values
  through both, 0 differ.
Screenshots of every run (`/tmp/shots/<run>/`) compared: no visual difference (the band of purple glyphs at the clouds of
erasure is the game's: it is also there with `SSBU_VK_VRC=0`).

**Left** (Tokyo, 700 s runs of the last build):
- Trains of 45-50 ms frames (7-25 frames a run): the same tree walk, `f_71007e1fe4` itself now (about 35 ms a frame), a
  large lifted function with float math. Not changed: the register cache (`RECOMPS_REGCACHE`, off since finding 91) or a
  host version of the walk would be the next step.
- One frame of 80-105 ms every 37.3 s (2,236 frames), each the first frame of a run of tessellated draws (the
  tessellated scene coming back into view); not the pipelines (three, made early). The game's main and job threads do
  more work in that frame (buffers made, `nvnBufferInitialize` under a game mutex, lifted code); not attributed further:
  the sampler gets about 25 samples a thread over three such frames. A table lock split 64 ways (`nvn_stub.c nobj`) was
  tried: no measurable change, not kept.
- Tools added on the way: `SSBU_FRAMETIMES`, `SSBU_HITCH` with a 19 ms threshold for late frames, `SSBU_LPROF` with 4M
  samples and the presented frame of each sample (`@<frame>`; `scripts/lprof_report.py` reads both forms).

## 60 fps menus and cutscenes (2026-10-25)

`cutscene_fps = 60` (graphics.ini; `SSBU_CUTSCENE_FPS=60` wins; default 30, as on the console) runs the title, menus and
cutscenes in the game's 60 fps mode.
- **How the game times them**: the main loop (`7100670b80`) asks each frame for a frame-rate mode from its state flags
  (0 = 60, 1 = 30, 2 = 15; set by `7100d02de0`, applied at `7100cfd9b8`). Its per-frame clock `7100c1f600` keeps the
  frame's step at clock+0xe0: in gameplay a ratio of the measured frame time, in mode-1 scenes (flags 28 and 30) a fixed
  1.0, one 30 Hz frame: a cutscene advances one step per game frame (2,758 frames at 30 or at 60). Forcing only the mode
  played the same cutscene in 46.6 s instead of 92.1 s.
- **What is done**: two host overrides (`lifter/recomp_eh.py` `HOST_OVERRIDES`, `runtime/game_shim_eh.c`):
  `h_fps_mode_set` makes a 1 a 0, and `h_frame_clock` replaces the fixed step while such a scene runs at 60 by the last
  frame's measured length (the clock's own, +0xf0) in 30 Hz frames, at most 1.0: 0.5 on time, more for a late frame, so the
  scene keeps real time when frames are late; a frame longer than 33 ms slows it as on the console (a measured gameplay
  step is left alone). (A fixed 0.5, the first version: 97.9 s for the cutscene at 56 fps in 4K.) The game's frames-to-seconds conversion (`7100cb4f54`) reads the mode it now has. Each change of the
  mode the game asks for is logged with the wall-clock time ("fps mode:" lines).
- **Measured** (Shibuya save, the cutscene that starts after 75 s on the street; `SSBU_FRAMETIMES` has the wall clock
  per frame): 30: 2,758 frames in 92.07 s; 60: 5,496 frames in 92.33 s, 59.5 fps, p99 22.0 ms (fixed half step); with the
  measured step 91.94 s at 59.7 fps, and 92.03 s at 56.4 fps in 4K (resolution scale 2.4). Screenshots at equal
  wall-clock times after the start (+1 .. +20 s, `/tmp/shots/cutscene_30_vs_60.png`): the same shots, poses and subtitle
  lines. Consecutive frames at 60 all differ (no repeated 30 Hz poses). Title screen and the main menu's entry at equal
  times: the same (`/tmp/shots/title_30_vs_60.png`). The pause menu is mode 0 anyway (not changed). The prologue movie
  (`ev0000.webm`, VP9, 30 fps): movie time against wall clock, 160.0 s -> 160.11 s at 30 and 160.09 s at 60, all 4,900
  frames: movies keep their rate (the game's player times them by the system clock).
- Not checked by ear: audio is muted in the test runs; the cutscene's timeline keeps wall-clock time, which is what
  the voice and lip sync follow.

## Resolution scale (2026-10-25)

`resolution_scale` (graphics.ini; `SSBU_VK_SCALE` wins; 0.25 .. 3.0, Vulkan) now scales B3's picture. The scale is relative
to B3's window texture, 1600x900: 1.2 = 1920x1080, 1.6 = 2560x1440, 2.4 = 3840x2160 (3.0 = 4800x2700). The picture goes to
the window at the window's size (graphics.ini `width` / `height`, `SSBU_WINDOW=<w>x<h>`; above twice the window it is
halved in 2:1 steps first).
- **What was missing**: the screen class (the images made at the scale) took only 1600x900 and its halves. B3 draws its
  scene into 1600x896 targets (1440x816 when its dynamic resolution steps down; depth-stencil, R11G11B10F, R32F, RG8) with
  chains of halves to 45x25 and some other 16:9 targets (1280x720, 1120x628, 960x540 ...): only 13 images were scaled.
  Now any target of the window's shape (within 1.5 %), 40 x 22 or larger, that is not an uploaded texture: 86-117 a run.
  Shadow maps (1024 / 2048 square, a 2048 depth array) and other odd sizes stay as they are.
- **Storage images**: fragment programs #1693 and #5548 write 800x448 / 720x408 / 512x288 targets with SUST at
  gl_FragCoord in the game's pixels; such an image is made again at the game's size the first time it is bound as a storage
  image and kept there (`g_rs_keep`, 4-6 a run; the shaders that sample it use normalised coordinates).
- **Compute**: B3's compute dispatches touch no scaled image (one 64x64 storage image; `SSBU_VK_RTLOG=1` lists the render
  targets, every stage's storage images and any scaled image a compute stage samples), so their sizes need no change.
  Viewports, scissors, clears, copies, uploads, read-backs, `gl_FragCoord` and integer texel fetches in fragment stages
  were converted already (track O).
- **Measured** (Shibuya benchmark, 330 s, frames at 60 from 1500, window at the output size; screenshots of the scaled
  image itself with `SSBU_VK_SHOT_NATIVE=1`, `/tmp/shots/res<scale>/`, `/tmp/shots/hud_1x_vs_4k.png`):

  | scale | output | avg fps | 1 %-low | p99 | frames at 60 | scaled images |
  |---|---|---|---|---|---|---|
  | 1.0 | 1600x900 | 59.9 | 51.7 | 19.3 ms | 98.0 % | - |
  | 1.2 | 1920x1080 | 59.9 | 49.3 | 20.3 ms | 97.6 % | 259 MB |
  | 1.6 | 2560x1440 | 59.8 | 47.9 | 20.9 ms | 97.2 % | 385 MB |
  | 2.4 | 3840x2160 | 59.6 | 47.5 | 21.1 ms | 96.0 % | 976 MB |

  The pictures: the scene, fog, effects, the HUD, subtitles, the title, the main menu, loading screens, a cutscene and the
  game-over screen drawn at the scale, nothing misplaced or cut. In 4K the 60 fps cutscene runs at 56.4 fps (p90 21 ms).
- **graphics.ini on Linux**: until this round the file was never read on Linux (the program's folder was cut at '\'
  only); now `config/graphics.ini` is (resolution_scale and cutscene_fps from the file checked). The tracked file sets
  `anisotropic_filtering = 8` (from B2recomp), which now applies on Linux too; its `resolution_scale` was 2.0 (B2's
  2560x1440; 3200x1800 for B3) and is back to 1.0. `controllers.ini` (nnpad_sdl.c) had the same cut: fixed in the keyboard round below.
- **The clouds of erasure** (the Homunculi's smoke, Shibuya; blurry at 4K in the user's play): tessellated patches drawn
  into fixed 520x360 targets (two RGBA16F + D16 and a 3-layer RGBA16F array; then a full-screen composite). 520x360 is
  not of the window's shape: named in `rs_extra_class`, scaled with the others (1248x864 at 2.4). Crops at 4K
  (`/tmp/shots/smoke_before_after_2700.png`): the blocky, dotted cloud edges against geometry are gone, the composite
  stays aligned. `SSBU_VK_CLOUD_SCALE=<cap>` limits their scale for a weak GPU (default: the full scale; 4K A/B at equal
  load, 1.0 against 2.4: 78-81 % against 75-77 % of frames at 60).
- **Measurements later on 2026-10-25 were taken with other programs loading the machine** (a game in Java at about two
  cores and the GPU, two long-running emulator processes): 4K runs then gave 75-81 % of frames at 60 whatever the setting
  (the 96 % above was an idle machine). Only A/B runs interleaved under the same load are compared.
- **First run at a scale other than 1.0**: the fragment stages are another translation (one for every scale, keyed
  `rs1`); B3 has no start-up shader preparation (no shader containers), so the first such run translated 3,755 of them
  behind held loading screens: 110.7 s of holds, once per GPU and driver. Later runs at 1.2, 1.6 and 2.4: none.

## Level of detail (2026-10-25)

- **`texture_lod_bias`** (graphics.ini; `SSBU_TEXTURE_LOD_BIAS`; -3 .. 3, default 0 = the game's own): added to the mip
  level of every sampler that uses mip maps (clamped to the device's limit). At -1 the same view is sharper (high-frequency
  energy: near street 113 -> 153, far street 273 -> 292, distant buildings 292 -> 340); no measurable cost (4K A/B:
  77-80 % against 77-79 % of frames at 60).
- **Model LOD: not found yet.** Candidates read and tested with a host override that scaled their values, measured by
  the indices a frame draws (`SSBU_VK_PRIMLOG`) on the same idle view:
  - `LodDistance0..3` / `UseLodDistance0..3` (reader `71007d3a5c`, pairs of float + flag): the values seen are 0, -1, 0.5,
    0.25, 5 / 15, 10.8 / 26, and every Use flag is 0: an effect system's settings, not models;
  - `Lod { Lod0..Lod6 }` beside `Deletable { max, extend, vanishTime }` (reader `7100646060`, seven floats at
    +0xc0c..+0xc24 of one object): read once, 0 10 20 30 50 80 100. Scaled by 0.2 and 3: 1.01 / 1.04 million indices a
    frame against 1.02-1.07 million at 1 (two runs): no effect on this view (likely the breakable props' table);
  - `LOD0..LOD7` names (`71007f44ac`): an enum parser of level names (models' LOD groups or settings), reached through a
    function pointer.
  Next: find a model whose index count changes with distance (a scripted walk away, `SSBU_VK_PRIMLOG` per draw and
  vertex buffer), then the code that picks its batch list. The temporary overrides were removed.

## Frame rates above 60: feasibility (2026-10-25; a study, not implemented)

**How B3 times a frame** (read off the lifted code; the same engine generation as Bayonetta 2):
- The per-frame clock `7100c1f600` (called by the main loop `7100670b80` before it picks the frame-rate mode) measures
  the last frame in ms (+0xf0) and keeps the frame's **step** at +0xe0, in 60 Hz frames:
  - gameplay (w1 = 1): the mean measured frame time since the last step / the ms of a 60 Hz frame (+0xec), minus a
    carry (+0xe4), **clamped to 1.0 .. 2.0** (the rest kept as carry); a global override (`7100ba3300`, a float that
    wins when above 0);
  - title, menus, cutscenes (w1 = 0, state flags 28 and 30): a fixed 1.0 = one 30 Hz frame (`cutscene_fps = 60`
    replaces it by the measured length, `h_frame_clock`).
- `7100c1f72c` multiplies the step into five chained **time channels** (scales at +0x88 .. +0xcc, scaled steps stored at
  +0x94 ...): the slow motions / Witch Time / pause of the game read these.
- So gameplay already moves by a measured delta between 30 and 60 fps (a 25 ms frame = a step of 1.5); above 60 the floor
  of 1.0 makes the game run faster than real time (at 120 fps, twice as fast). This is the clock of Bayonetta 2
  (B2recomp: `0x7100dd1d60`, the same 1.0 / 2.0 bounds, five channels).

**What B2recomp's 120 fps beta needed** (its `scripts/fps_patch.py`, `lifter/fps_ticks.txt`, `lifter/fps_smooth.txt`;
v0.2.0 beta on 2026-10-17, v0.3.4 on 2026-10-23, about 26 commits): the clock always measures and its floor goes to
1/16; the present asks for interval 0 and a limiter paces; about 60 **per-frame counters** (player / enemy countdowns,
combo windows and the combo display, Witch Time's end, input windows after a press, stick-motion moves, enemy sound
timers) advance by the 60 Hz frames of game time due ("smart tick", per site and field address); 48 **smoothings**
x += k (T - x) take their closed form T + (x - T)(1 - k)^step; float timers keep their overshoot; cinematic shot / scene
holds keep their game time; the slow motion's end lands on the frame grid. The counters were found by a **census**:
per-site hit counts while the frame rate alternates 60 / 120, then read one by one.

**For B3** (what carries over, what does not):
- The clock floor, the present interval and the runtime's pacing at 120 (`frame_rate`, display.c `gfx_pace_rate`, from
  Recomps#1) carry over almost as they are: about a day.
- Every counter and smoothing address is B3's own (another binary): the census tooling (site counters in the lifted
  code, the alternating-rate run) has to be rebuilt here, and each site read. B2's patterns (the smoothing form, the
  overshoot form) can be searched for the same way in B3's lifted C. Expect a list of similar size (B3 has more
  systems: Demon Slaves, Viola, Jeanne's levels).
- Known frame-locked parts so far: the 30 fps-mode scenes (cutscenes and menus: their own step, handled); the
  frames -> seconds conversion by mode (`7100cb4f54`); the pad is read once a frame (input windows count frames:
  the B2 kind of fix). Physics and animation are not read yet: if they take the scaled step they follow; anything that
  counts frames does not.
- Performance: 120 fps needs 8.3 ms on the critical path. Today's late-frame analysis (Shibuya, Tokyo) has frames of
  16-20 ms of game + render work in heavy scenes (render thread 4-11 ms, the main thread's own work and its jobs, plus
  long frames in lifted code: the tree walk `71007e1fe4`); 120 would hold in light scenes only, 80-100 in fights.
- Risks: subtle gameplay timing (combos, dodge offset, Witch Time windows, enemy attack timers) that only shows in
  play; cutscene sync (handled for the 60 case); effects that count frames.
- Estimate: the uncapped clock and pacing about a day; a playable beta with the census and the first ~50 sites about a
  week of sessions (B2's took a week after its research); then fixes as play finds them.

## RomFS archives: the game's own Kraken decoder (2026-10-25)

`build/b3-game targets/exefs/main.elf --pkz-dump <folder | -> [filter...]` (cwd `build/<run>`, `SSBU_ROMFS` as for a
run; `runtime/pkz_dump.c`) writes every `.pkz` entry to `<folder>/<entry name>`: Kraken entries decompressed by main's
own Oodle decoder (lifted), stored ones copied. No window, no audio, no game code but the decoder (image, relocations and
import handlers only); 8 threads (`SSBU_PKZ_JOBS`); a filter keeps entries whose archive or entry name contains it; `-`
decodes without writing. Entry names are unique across the archives, so the folder mirrors the game's paths. Write it
under a git-ignored folder (`cache/pkz/`): the files are game assets.
- **All 72 archives**: 18,732 entries (11,760 Kraken, 6,972 stored), **0 failures**: every Kraken stream used exactly its
  stored size and gave exactly its recorded size (`pkz.py check targets/romfs --dump cache/pkz` agrees for every entry).
  25.8 GB: 10.7 s decode only, 45.4 s with the writes (8 threads on a 16-thread CPU, NVMe, the archives partly in the page cache).
- **Content**: every Kraken entry has its type's magic (6,408 `.dtt`, 2,986 `.dat`, 1,305 `.eff`, 262 `.evn` = `DAT`;
  161 `.bin` = `PTD`; `EPB`, `WME3`, `VAT`, `VFD`, `WTB`, `BXM`; the 327 `.wtp` are headerless texture data); all 15,526
  DAT containers of the dump parse with 130,976 members inside their files. `ui/ui_option.dat` (the Options screen) holds
  13 `option00xx.uid` layouts plus `option.wta` / `.uvd` (PlatinumGames UID, as guessed before); `Text/UI/
  core_pln_USen.bin` gives 4,672 readable strings.
- **Stored replacements**: a `Text_000.pkz` rebuilt with all 394 entries stored (`None`), in `mods/<name>/romfs/`, is read
  by the game; its changed copyright line showed on the title screen. So rebuilt archives need no Kraken encoder.
- **Mods folder on Linux**: it had the `''`-only cut fixed for graphics.ini in 028ea6d (`nnfs.c`: the folder and
  `mods.ini`), so mods worked only with `SSBU_MODS`; fixed, the default `mods/` beside `build/` is found.
  `controllers.ini` (nnpad_sdl.c) and `dlc.txt` (nnmisc.c) still have that cut.

## Keyboard and mouse (2026-10-25)

The keyboard + mouse path came from B2recomp (nnhid.c, nnpad_sdl.c, display.c) and was never used on Linux:
`config/controllers.ini` was not read (the program's folder was cut at '\' only, as graphics.ini), and its defaults were
Bayonetta 2's. Now:

- **controllers.ini is read on Linux** and reloaded within a second of being saved (rebinding while the game runs).
- **B3 defaults**, from the game's own Options -> Controller page (Type A; `/tmp/shots/kbm_game_controls_typeA.png`):
  left mouse = X punch, right mouse / Enter = A kick / confirm, Space / Backspace = B jump / back, E = Y shoot,
  left Shift = ZR dodge, Q / mouse 4 = ZL Demon Slave, R = L weapon switch, F = R stance change, middle mouse / V =
  right-stick press (camera / lock-on), C = left-stick press (waypoint), arrows or 1 2 3 / T = D-pad (Demon Select /
  taunt), Tab = Minus (subscreen), Esc = Plus (pause), WASD = left stick, I J K L = right stick, left Ctrl = WALK.
  One line per action in the file, two keys each at most; a key bound to two actions is named in the log.
- **Movement**: key diagonals have the length of a straight push; WALK holds the stick at 0.4 (below the stick-as-button
  bits at 0.5). **Camera**: mouse movement becomes right-stick deflection proportional to its speed (1200 counts/s =
  full, times `mouse_sensitivity`; `mouse_invert_y`). The pointer is captured (SDL relative mode) after a click into
  the window (at once when fullscreen); that click is not given to the game; Esc or losing the focus releases it.
- **Pad and keyboard together**: both drive player 1 at once (buttons add up; a pad's stick wins while it is off
  centre), so switching is just using the other device. The button prompts stay the game's Switch ones: there are
  no keyboard pictures in the game (pad prompts: `button_prompts` as before, chosen at start).
- **Linux key state** comes from the window's SDL events (window thread), not from SDL_GetKeyboardState on the game's
  thread; cleared when the focus goes.
- Fixed on the way: with a pad on the keyboard's player, a vertical-only mouse move was replaced by the pad's centred
  stick; an unmapped joystick's axis now drives a stick only after it was seen near the centre (this machine's
  Swiftpoint Z3 mouse receiver is a joystick with five axes resting at -32768: it was player 1 with both sticks held,
  which would override WASD and the mouse once the window had the focus).

Tests:

- `SSBU_PAD_SELFTEST=1 build/b3-game`: 61/61 (was 49), new: every default key gives exactly its Pro bit, WASD,
  diagonal, walk, mouse to right stick (both axes), pad + keyboard together, rebinding by editing the file while
  running, invert / sensitivity, the pinned-axis receiver.
- **Game runs with synthetic events** (`SSBU_KBM_SCRIPT`, below; muted, hidden window, a copy of the save, no other
  input): Enter through the title into Shibuya; capture click swallowed; run, diagonal, walk; camera turned and tilted by
  the mouse; punch, kick, shoot, jump, dodge, lock-on, taunt; Tab subscreen and Backspace back; Esc pause, arrows / S /
  Enter / Backspace through the pause menu and Options; X rebound to U in the run's file at 150 s: U punches, the left
  mouse no longer does. The log line of each step and the Npad state the game read match. 60 fps in the free roam of
  that run. Pictures: `/tmp/shots/kbm_*.png`.
- `SSBU_KBM_SCRIPT="<s>[+<dur>]:<input> ..."` (Linux, test only): pushes SDL key / mouse-button / motion events
  (`MOUSE1`, `W`, `M<dx>,<dy>` = motion every 10 ms) into the window's queue, seconds from window creation; the window
  counts as focused, and the keyboard player's Npad state is logged at each change (`kbm: ...`).

Not tested: a person at a real keyboard and mouse (camera feel, sensitivity default), Wayland / X11 pointer capture of a
visible window (the test window is hidden, so SDL's relative mode was not switched there), Windows (not built).

## Next blockers (most urgent first)

Camera (right stick): confirmed working by hand on a real pad (2026-10-24); no work needed.

1. **Which route a run takes** depends on the save and on when the A presses land on the title (movie route, Chapter
   Select → Prologue, Chapter Select → chapter 1). An `SSBU_SEQ` with `@draws` anchors would make it reproducible.
2. **Long frames left in Tokyo** (section "Performance in Tokyo"): trains of 45-50 ms in a tree walk of lifted code, one
   80-105 ms frame every 37 s where the tessellated scene comes back. Fights and Tokyo otherwise hold 60 (98 % of frames).
3. **Picture correctness**: no reference images to compare with.
4. **Windows build** of the movie changes and of the b3-game rename (not compiled here).
5. **Barriers in the translator's dispatch loop** (`SharedMemoryDataRace-RaceOnLoadStoreVsAtomic`, program #7839):
   structured control flow for stages with `BAR`.
6. **First-run warm-up**: minutes of held loading screens after a translator change.
7. The static scene after `SSBU_SKIP_MOVIES` (stage 5), `nn::account::PopOpenUsers` (called once, harmless), the
   `nn::audio` Resampler imports (not reached), about 170 other imports without a handler (none reached).

## Known shortcuts (marked `shortcut:` in the code)

- `SdkMutexType`, `pthread_once`, `std::__call_once` wait by spinning on `Sleep(0)` (no kernel wait queue).
- `nn::fs` thread fs-context is a shared dummy object; `nn::oe` notifications / recording and `hid` mouse are no-ops.
- `--pkz-dump` calls main's Oodle functions at fixed addresses (checked against two instruction words at start: other
  builds of main stop with a message); it holds a whole entry in memory (the largest is 373 MB).
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
- Tessellation: a tessellated pipeline is made on the render thread at its first draw (one hitch; the driver's pipeline
  cache keeps it); generic patch attributes and a vertex stage with clip distances in a tessellated program are not
  handled (none in B3); the picture of the tessellated object was not compared with a reference.
- Not tested: Windows builds of these changes (only `TOOLCHAIN=linux` was built and run).
