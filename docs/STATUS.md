# b3recomp status

End of stage 1 (2026-10-23). Linux only (the system's clang 23, SDL3, Vulkan, glslang), measured on the developer's PC
(16 threads, RX 9070 XT / RADV) with their own dump.

## What builds

`python scripts/setup.py --dump <dump>` runs all nine steps and they pass:

| step | result |
|---|---|
| deps, verify, convert, mods | the dump is the expected build (main build id `b47cc79b...`); main, sdk, subsdk0 -> ELF, segment hashes ok |
| tables | `runtime/gen/*` from the player's sdk/main: layout, 600 imports, 24 lifted SDK functions, 90 of 105 NVN device answers |
| lift | 55,277 functions, 5.1 M instructions -> 407 C files, 5 s |
| compile | all 409 files compile, 0 failures (about 12 min with 15 jobs on a loaded machine; 47 large functions fall back to lower optimisation) |
| link | `build/game.so` (154 MB) + `build/game_native` (4.4 MB) + `libssbu_glslang.so` |
| check | B2's lifter self-test (382 pair words, 25 SIMD words) and the B3 one (200 words, 4,000 cases) against Unicorn: 0 failures (needs `pip install unicorn`; `build/venv` is fine) |

Tools: `scripts/pkz.py` (list / check / extract `.pkz`; checked over all 72 archives), `scripts/pgdat.py` (DAT),
`scripts/nvn_procs.py` (NVN entry points of the sdk), `scripts/runtime_coverage.py` (imports vs handlers).

## How far it boots

```
cd build && mkdir -p run && cd run
SSBU_ROMFS=$PWD/../../targets/romfs SSBU_USERDATA=$PWD/userdata ../game_native ../../targets/exefs/main.elf 20
```

(argument 2 = seconds before the runtime stops it; the log goes to the console, the first calls of every import to
`boot_trace.txt`.) The process, its window and its audio stream are named **B3 Recomp** (app id `b3recomp`), so they do not
clash with B2recomp's `game_native`: `pgrep -x "B3 Recomp"` finds it. It now gets through:

1. image mapped, 180,844 relocations applied, text/rodata write-protected;
2. **phase 0** `DT_INIT` (musl `_init` -> `__nnmusl_init_dso`: main's TLS registered);
3. **phase 1** `nninitStartup`: 2.98 GB heap, `StandardAllocator`, thread-local allocator;
4. **phase 2** all 265 static constructors (fonts, hid vibration nodes, mutexes);
5. **phase 3** `nnMain`: memory blocks (3,028 MB in 9 blocks), time, oe (language, docked / boost), CPU boost, sockets,
   protobuf / gRPC (NPLN) initialisation, hid (npads, vibration devices, six-axis, touch, keyboard; SDL3 pads seen),
   **RomFS mounted** as `Contents:`, `Contents:/` and `movie/` listed, **all 72 `.pkz` opened and their tables read**,
   9 game threads started (`FileContentsWork`, `FileContentsRead`, `MainDraw`, 3 x `MainJob1`, 3 x `MainJob2`),
   `nv::InitializeGraphics`, `nn::vi` display and layer, NVN bootstrap, device (API 55.12), queue, five memory pools,
   device limits, GPU heaps including the 262 MB frame-buffer heap;
6. **stops** with a guest trap when `runtime/nvn_sdk.c` asks the sdk for a texture's storage size at B2's SDK 3.6.0
   address (0x2c9748; in B3's sdk that is libpng code). No frame is presented yet.

98 different imports are called up to that point, all with handlers; 37 NVN entry points are called.

## Next steps (the blockers, most urgent first)

1. **Port `runtime/nvn_sdk.c` to NVN 13.3.2.** It runs the SDK's own texture-storage rules (size, alignment, storage
   class, sparse tile layout) by address, against B2's builder / texture object layouts and its GM200 HAL model.
   `scripts/nvn_procs.py` gives the 13.3.2 addresses (`nvnTextureBuilderGetStorageSize` 0x4b66e0, ...); they become
   `addr` roots in `lifter/sdklift_roots.txt`, and the builder / texture layouts and the HAL path must be re-read in
   B3's sdk. This is the current stop.
2. **NVN 55 entry points**: 32 that the runtime lacks (transform feedback, separate textures / samplers, deferred binds,
   subroutines, `MapVirtual`, `GetViewOffset`, subtile). Each traps with its name when first called.
3. **Shaders**: B3's shader archives are in `root_000.pkz` (`shader.dat`, `shadereff*.dat`); NVN reports GPU code
   1.5-1.16. The Maxwell -> SPIR-V path came from Smash (SDK 13) and should fit, untested. The runtime's shader
   preparation still looks for Smash's `data.arc` (harmless, but nothing is prepared ahead).
4. **Movies**: `movie::` (subsdk0) plays VP9 WebM; the runtime has only a failing placeholder on Linux (Media
   Foundation on Windows). Needs a VP9 decoder behind `movie::Extractor` / `Decoder` (libvpx or FFmpeg).
5. **Audio**: Wwise 2021 on `nn::audio` (renderer from Smash's runtime) plus 7 missing imports (`AudioOut`, Resampler).
6. **Online (NPLN)**: gRPC / protobuf initialise offline; `nn::ssl` (4), `nifm` / `nsd` answers for "no network".
7. **The rest of the imports**: 174 without a handler (mostly libc stdio / dirent / rwlock / time and libc++ streams,
   easy on the host libc). `runtime_coverage.py` lists them.
8. **Compile time**: about 12 min; a few huge switch functions dominate (see `build/obj/_report.txt`).

## Known shortcuts (marked `shortcut:` in the code)

- `SdkMutexType`, `pthread_once`, `std::__call_once` wait by spinning on `Sleep(0)` (no kernel wait queue).
- `nn::fs` thread fs-context is a shared dummy object; `nn::oe` notifications / recording and `hid` mouse are no-ops.
- `scripts/pkz.py` decompresses Kraken entries only with an Oodle library the user supplies.
- The heap size (3,285 MB) is the console's documented application memory, not measured on hardware.
- Not tested: Windows builds of these changes (only `TOOLCHAIN=linux` was built and run).
