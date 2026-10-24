# Bayonetta 3 (Switch): reverse-engineering notes

Findings of stages 1 and 2 (2026-10-23), from the developer's own dump. Numbers below are sizes, counts, versions and offsets
inside the modules; no code or data of the game is reproduced. Offsets are module offsets (main at `0x7100000000`,
sdk at `0x7500000000` in the runtime). Everything here can be re-checked with the tools named in each section.

## 1. The ExeFS

| file | size (bytes) | NSO build id | segments (text / ro / data, uncompressed) | what it is |
|---|---|---|---|---|
| `main` | 16,894,145 | `b47cc79b88b7091ea514b2b725c50ae01c6f00df` | 0x1323400 / 0x82a7cc / 0x968688, bss to 0x59e2000 | the game |
| `sdk` | 5,989,236 | `357efc198e728cb5d55265aff304f03cfc9cc0e3` | 0x5d4bd0 / 0x6ad878 / 0x66c88 | `nnSdk` 13.3.2, `NVN` 13.3.2, `libz` 13.3.2 |
| `subsdk0` | 3,385,425 | `a8cb97803f7f442a3b6cce1c7cd4ee4a78b7def3` | 0x33e7d0 / 0x210194 / 0x30310 | `multimedia.nss`: `NintendoSDK_movie-13_3_2` (Android-Stagefright based `movie::`) |
| `rtld` | 7,132 | `567cdd4c5251494c5cd70822e3ccc740c190c2b2` | | the loader |
| `main.npdm` | 1,604 | | | program id `01004A4010FEA000`, "Application", main thread priority 44, core 0, stack 0x100000 |

- All three segments of every NSO are LZ4-compressed with SHA-256 hashes (flags `0x3f`); B2recomp's
  `tools/extract/nso2elf.py` converts them unchanged and the hashes match.
- main: MOD0 at text+8; DYNAMIC at 0x249b028; 626 dynamic symbols = 600 imports + 26 exports (`nnMain`,
  `nninitStartup`, `malloc`/`free`/`calloc`/`realloc`/`aligned_alloc`, `operator new`/`delete`, a few typeinfo and SDK
  class destructors). `DT_NEEDED`: `multimedia.nss`, `nnSdk.nss`. 180,844 relative relocations, 2,318 to imported
  symbols, 10 undefined weak (null). `DT_INIT` = 0x30 (musl `_init`), `DT_INIT_ARRAY` 265 constructors.
- main has **C++ exception tables**: 54,550 FDEs, 3,039 functions with an LSDA, 10,549 landing pads, 392 functions with a
  catch clause. B2's main had none (B2recomp linked `game_shim.c`; B3 needs `game_shim_eh.c`).
- Middleware linked into main (from its version strings): NPLN 1.2.0 with Google APIs / gRPC / OpenSSL / protobuf
  (Nintendo's online platform), Wwise 2021.1.7.7796 with PlatinumGames plug-ins and Auro headphone, Opus decoding,
  the Oodle Kraken decoder (section 4), libpng.
- npdm service access list: `acc:u0 aoc:u apm appletOE audin:u audout:u audren:u banana bcat:u bsd:u bsdcfg caps:su
  caps:u csrng ectx:aw friend:u fsp-srv hid htc htc:tenv htcs hwopus irs ldn:u ldr:ro lm mii:u mm:u nfc:mf:u nfc:user
  nfp:user ngct:u nifm:u nim:eca notif:a nsd:u ntc nvdrv pcm pctl pl:u prepo:u set sfdnsres ssl time:u vi:u`.

### Compared with Bayonetta 2 (B2recomp's target)

| | Bayonetta 2 1.2 | Bayonetta 3 (this dump) |
|---|---|---|
| nnSdk / NVN | 3.6.0 / 3.6.0 | **13.3.2 / 13.3.2** (NVN API 55.12, section 6) |
| main .text | 0x120dbe0 | 0x1323400 |
| imports of main | 549 | 600 (297 shared with B2) |
| C++ exceptions in main | none | 10,549 landing pads |
| static TLS in main | none used | 88-byte block (section 5) |
| RomFS | CRI `.cpk` + loose `sound/` | PlatinumGames `.pkz` (section 4) + `movie/*.webm` |
| movies | via `movie::` | via `movie::` from `subsdk0`, VP9 WebM 1600x900 30 fps, no audio track |

The upstream of B2recomp's runtime (ssbu-recomp) was written against Smash's SDK 13.0.4, so several of its handlers fit
B3's SDK 13 better than B2's 3.6.0 did (e.g. `nvnBootstrapLoader` / proc names, the 13.x `nn::os` types).

### Version check

The dump has no control data (no NACP); the game itself shows **1.2.0** on its title screen. The port identifies the
build by main's build id. `scripts/setup.py` (step `verify`) holds the SHA-256 and size of each ExeFS file, three small RomFS archives
and a manifest hash of the RomFS file list (552 files, 15,047,510,227 bytes), the same way B2recomp checks for 1.2.

## 2. Imports the runtime must provide

`python scripts/runtime_coverage.py runtime targets/exefs/main.elf <strings.tsv> <out.tsv>` lists every import with the
runtime file that handles it. Copied from B2recomp unchanged, the runtime handled 375 of the 600. After stage 1:
402 by host handlers + 24 lifted from the player's sdk (`lifter/sdklift_roots.txt`) + the `nn::util` constants
(`runtime/gen/sdkdata.c`). 174 have no handler yet; none of them is reached before the point where boot stops
(section 7). By family (handled / imported, stage 1 end):

| family | imported | no handler yet | notes |
|---|---|---|---|
| libc / C | 174 | 64 | stdio / dirent / pthread_rwlock / time functions; host libc can back most |
| libc++ / C++ ABI + other C++ | 120 | 56 | iostream pieces, `to_string`, typeinfo / vtable data symbols, `movie::` (18, from subsdk0) |
| nn::os | 86 | 13 | ReaderWriterLock, TimerEvent, MultiWaitHolder for timer / system events |
| nn::hid | 38 | 1 | VibrationNode classes lifted from the sdk |
| nn::audio | 20 | 7 | AudioOut and the Resampler |
| nn::util | 16 | 8 | pmr done; data constants via sdkdata.c; UTF conversions lifted |
| nn::socket / ssl / nifm / nsd | 27 | 12 | online (NPLN): offline answers needed |
| nn::oe / time / prepo / err / account / pl / diag / fontll | | 13 | mostly SDK 13 variants of calls B2 had in an older form (nn::fs: all handled) |

NVN: main names 533 NVN entry points (it resolves them through `nvnBootstrapLoader`); the runtime has 501. The 32
missing are NVN-55 additions: transform feedback (9), separate textures / samplers (6), deferred binds (5), program
subroutines (2), checkpoints (6), `nvnMemoryPoolMapVirtual`, `nvnTextureBuilderGetViewOffset`, subtile size (2). The game
fetches all of them at start; whether it calls any is not known yet (calling one traps with its name).

## 3. The lifter on B3's main

- `lifter/recomp.py` reads the layout from the ELF (B2's constants are gone). `recomp_eh.py` lifts main with landing-pad
  resume entries in 5 s: 55,277 functions, 5,122,339 instructions, 407 C files (~0.6 GB).
- New AdvSIMD forms (about 40 encodings, 1,100 sites): integer min/max, shifts by register (`ushl` is the most common),
  saturating add/sub, `mla`/`mls`, pairwise and across-lane sums, add/sub long/wide, `frint*`, `fcvta*`, `{s,u}sra`,
  `{r}shrn`, `orr`/`bic` immediate, integer by-element multiplies, one-lane `ld2`-`ld4`/`st2`-`st4`, `ld2r`-`ld4r`.
  Checked against Unicorn by `lifter/test_b3.py` (200 words, 4,000 cases). What is left untranslated is zero padding,
  the MOD0 header and `udf` traps.
- **Jump tables**: B3's compiler emits `adr xB, base; ldrb/ldrh wE, [table, idx]; add xB, xB, wE, lsl #2; br xB`, with up
  to 20 other instructions scheduled in between. The `adr` target was taken as a function entry (B2's lifter adds every
  `adr`/`adrp+add` code address), which cut each switch function in two. Now a code address whose register reaches a `br`
  within 24 instructions, with no write of that X register (SIMD / FP destinations do not count) and no branch in between,
  is not an entry; none is left in main.
- B2's `test_ext.py` expected `ldaxp xzr, ...` to write `x[31]`; it is a discard (the lifter was right, the test is fixed).

## 4. RomFS: `.pkz` archives

72 archives + `movie/` (478 `.webm`, 2 `.avi`, folders `tutorial/`, `skilltree/`, `gallery/`). `scripts/pkz.py` lists,
checks and extracts them; `scripts/pgdat.py` (from B2recomp) reads the DAT files inside.

```
header (0x20 bytes, little endian)
  0x00 char[4]  'pkzl'
  0x04 u32      0x00020000 (version)
  0x08 u64      file size
  0x10 u32      entry count
  0x14 u32      entry table offset (0x20)
  0x18 u32      size of the name table, which follows the entries
  0x1c u32      0x1000 (alignment of the data area)
entry (0x20 bytes)
  0x00 u32      name offset (into the name table)
  0x04 u32      codec name offset (into the name table)
  0x08 u64      size
  0x10 u64      data offset
  0x18 u64      stored size
```

- Codec names are strings in the name table: `"OodleKraken"` (11,760 entries) and `"None"` (6,972: the Wwise banks,
  text). A Kraken entry is one Oodle stream (header bytes `8c 06`). **Not zstd.** Oodle is closed source; main contains
  its own Kraken decoder, so the recompiled game decompresses its files itself. `pkz.py` extracts Kraken entries only
  with an Oodle library the user already has (`--oodle`).
- Entry names are paths (`core/core.dat`, `Text/BATTLE/bayo_EUde.bin`, `sound/bgm/BGM.bnk`); `root_000.pkz` holds the
  shader archives (`shader.dat`, `shadereff*.dat`, ...). The inner `.dat`/`.dtt` files are PlatinumGames DAT
  (`DAT\0`, as in B2), with `.bxm` XML; sound is Wwise `.bnk`.
- At boot the game mounts the RomFS as `Contents:`, lists `Contents:/` and `movie/`, opens all 72 archives and reads
  each header and entry table (seen in `boot_trace.txt`).

## 5. TLS

main reads `tpidr_el0` 206 times, with local-exec offsets +16 .. +96 (AArch64 TLS variant 1: 16-byte TCB, then main's
block). B2's runtime gave each thread a zeroed block, which worked for B2. B3's block has an initial image: musl's
`_init` (`DT_INIT`) calls `__nnmusl_init_dso` with (all relocated, so numbers arrive as image base + value):
`x0` image base, `x2`/`x3` .tdata start / end (16 bytes: `{-1, 0}`, protobuf's thread cache), `x4` base + alignment (8),
`x7` end of the block (.tbss end; 88 bytes in all). `runtime/b3_imports.c` records it and every thread block gets a copy.
(x5, x6 and the stack arguments are other linker symbols: init/fini arrays, RELA, JMPREL, HASH, DYNAMIC.)

## 6. NVN 13.3.2 inside the sdk

- **API version**: the game requires major 55 and minor >= 12 (main 0x7100d176c8).
- **Proc table**: NVN 13.3.2 keeps no `"nvn..."` strings. `nvnDeviceGetProcAddress` (sdk 0x4ac810) binary-searches 532
  names stored without the `nvn` prefix (u32 offsets at sdk 0xb21038 into the names at 0xb1d723, search at 0x4ac310)
  and takes the function from a parallel pointer array (GOT slot 0xce8030 -> 0xcd2bc0). `scripts/nvn_procs.py` prints
  all 532 from the player's own sdk (e.g. `nvnTextureBuilderGetStorageSize` = 0x4b66e0, B2's was 0x2c9748). NVX
  extensions are a separate `strcmp` chain (0x4abee0).
- **nvnDeviceGetInteger** (0x4a6db0 -> 0x4abd90): a switch with an `adr`/`ldrb` table; w0 preset to 55; a getter
  `bool f(pname, int *out)` (0x499b60) answers the fixed ones. `runtime/gen/make_gen.py` finds both by their constants
  and takes 90 of 105 answers (GPU code versions 1.5 .. 1.16, pool / descriptor sizes and alignments) from the player's sdk.

### NVN 13.3.2 objects, as the sdk's own code reads them (stage 2)

Found by disassembling the entry points `scripts/nvn_procs.py` names; offsets are into the opaque objects the game hands
over. The runtime keeps its own layout in those objects; these are what `runtime/nvn_sdk.c` rebuilds to run the sdk's
texture-storage code.

**NVNtextureBuilder** (the setters): +0x08 device, +0x10 storage pool, +0x18 offset, +0x20 flags, +0x24 target,
+0x28 width, +0x2c height, +0x30 depth, +0x34 levels, +0x38 format, +0x3c samples, +0x40 swizzle (4 words), +0x50
depth-stencil mode, +0x54 u16 packaged layout (1 | (word & 0xfff) << 4), +0x58 stride, +0x5c GL texture name, +0x60
packaged data. B2's NVN 3.6.0 had the same fields 8 bytes lower (device at +0) with stride and GL name swapped.

**NVNtexture** (nvnTextureInitialize, no GL name): no longer a copy of the builder but packed fields:
+0x08 device, +0x10 pool, +0x18 u64 offset (48 bits) | levels << 48 | samples << 56, +0x20 u64 width | height (16) << 32 |
depth (16) << 48, +0x28 48 bits: flags (16) | format (9) << 16 | target (7) << 25 | swizzle 4 x 3 bits << 32 |
depth-stencil mode (2) << 44 | format record +0xe != 0 << 46 | +0xf != 0 << 47; +0x48 the layout block (written by the
layout function, the first call of nvnTextureBuilderGetStorageSize): memory kind = byte +0x59, storage size = u32 +0x5a;
+0x60 GPU address, +0x78 u16 descriptor slot (only format 0x34). nvnTextureGetStorageClass = 0x100 | byte +0x59,
nvnTextureGetStorageSize = u32 +0x5a.

**NVNtextureView** is a packed 64-bit word in the sdk (nvnTextureBuilderGetViewOffset tests bits 56, 57, 61).

**Format table**: 0xa1 records of 0x30 bytes (B2: 0x34) behind a GOT slot; +0 bit 0 = exists, +8 / +9 block size,
+0xe / +0xf compression-related flags, +0x10 bytes per element. The table ends where the next table (with pointers) starts.

**NVNdevice fields** read by the storage code: +0xcc big page size (0x10000; nvnDeviceGetInteger also answers it),
+0x990 `{lib, NvRmGpuDevice*}` (the chooser reads +0x998), +0x1a10 / +0x1a14 compression switches (0 after
nvnDeviceInitialize; a value 0x76165656 disables compression), +0x1b30 kind of buffer / linear textures (0), +0x1b34 byte
"compression supported" (1, writer not found: inferred).

**Memory kind chooser**: NVN's helper builds an attribute record and calls `NvRmGpuDeviceChooseKind` -> NvRmNvgpuDevice
(`N8nvrm_gpu5nvgpu15NvRmNvgpuDeviceE`) method +0x30 = `return this->+0xf0` (the HAL object) -> HAL
(`N8nvrm_gpu33NvRmGpu_HwHalFunctions_GM200_BaseE`) method +0x90, which calls +0xa0, +0x120 .. +0x170 and reads the chip
descriptor's kind table. HAL object = 0x38 bytes {vtable, descriptor, 2 factory arguments, 0}; descriptor of the Tegra X1:
name "NVIDIA Tegra/Nintendo Switch" (twice) at +0 / +8, GPU id 0x12b at +0x10, factory at +0x18, kind table at +0x48
(2-byte entries), count at +0x50. `lifter/sdklift.py` lifts all of these by name (`nvn` and `vmethod` roots) and writes
their addresses to `runtime/gen/nvn_sdk_addrs.h`.

**Shader data** (nvnProgramSetShaders, NVNshaderData = {GPU code, control section}): the machine code starts at
data + control +0x708. Graphics stages: 0x30, behind magic 0x12345678 (program header at +0x30); compute stages: 0 (bare
code) or **0x100 behind a new magic 0x12345679** (66 of B3's 74 compute programs). control +0x6f8 = code size, +0x6fc /
+0x700 = size / data offset of the embedded constants, +0x714 = stage (5 = compute), +0x748 work group size.

**NVN 55 entry points** the game fetches (all 532 of the sdk's are fetched) that B2's runtime lacked: 32, now handled in
`nvn_gl.c` (see STATUS.md, shortcuts). Device and handle encodings (texture / sampler / separate handles) are the same as
in Smash's SDK 13.0.4 (checked in the disassembly).

## 7. Boot, step by step

What stopped boot, in order, and what fixed it:

1. Static constructors: a switch cut in two by the jump-table base -> lifter fix (section 3).
2. Constructors: hid Vibration classes, `SdkRecursiveMutexType::Initialize` without handlers -> lifted from the sdk.
3. nnMain, protobuf (NPLN) init: `CHECK failed` on its init-recursion test, because `__libcpp_thread_get_current_id`
   answered 0 -> host handler (plus `__call_once`, `pthread_once`, `sleep_for`).
4. The exception thrown by that check had no resume entry -> `game_shim_eh.c`.
5. protobuf arena: null serial arena from a zero thread cache -> static TLS image (section 5).
6. Graphics: NVN API 0.0 rejected -> 55.12; then zero pool alignments / GPU code versions -> sdk-derived answers (section 6).
7. GPU heaps: the 262 MB `FrameBufferHeap` did not fit: the game sizes its heap from the reported application memory
   (B2's 3 GB) and needs 3,028 MB of blocks -> 3,285 MB (the console's application memory).
8. NVN texture storage: B2's SDK 3.6.0 addresses -> the sdk's own 13.3.2 code found by name (section 6).
9. A switch at main 0xcc69c4 still cut in two: its `adr` base was 7 instructions before the `br` (the rule looked 6 ahead)
   -> up to 24, stopping at anything that writes the register or branches.
10. The game stops presenting after ~90 frames: the applet model suspends it when its window is not in the foreground
    (`SSBU_FOCUS_NOTIFY=0` for test runs).
11. `div` reached as a stub (garbage quotients) -> the lifted sdk `div` installed (`lifter/sdklift_install.txt`).
12. Crash while loading: main's state function at 0x6d760 was cut by a jump-table base whose `adr` and `br` had an
    `fmov s10` between them (taken as a write of x10) -> SIMD / FP destinations ignored; no table base is an entry now.
13. Compute stages translated from the 0x12345679 header instead of the code -> code at data + control +0x708.
14. `nvnCommandBufferDispatchComputeIndirect` missing in the Vulkan back end -> added. Title screen reached.
15. Story: `movie::Extractor(ContainerType, CacheSize)` (SDK 13 form) had no handler -> the failing placeholder; the
    opening movie is skipped and the prologue starts (3D world black, see STATUS.md).
