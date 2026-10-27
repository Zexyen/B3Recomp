# runtime/freestanding

The C library headers the player's PC compiles against: `string.h`, `math.h`, `stdio.h`, `stdlib.h` and `setjmp.h`,
cut down to what the lifted game code (`build/gen_eh/*.c`), `runtime/gen/sdkdata.c`, `runtime/gen/sdklift_gen.c` and
`runtime/wt_tables.c` use. `wtbuild` compiles those with `-nostdlibinc -isystem runtime/freestanding`, so the compiler
needs no libc development package: `stdint.h`, `stddef.h` and `stdarg.h` come from clang's own resource directory.

Each declaration is glibc's own, with the same attributes (`nothrow`, `const`, `pure`, `nonnull`), and the macros are
the ones glibc's `math.h` defines, so clang makes the same code from them: `tools/wtbuild/compare_freestanding.py`
compiles stand-in lifted code with these headers and with the system's and checks that every object file is the
same, byte for byte. The rest of the runtime (prebuilt in CI, or by `wtbuild` on a developer's PC) uses the system's
headers as before.

`jmp_buf` is glibc's x86-64 layout (200 bytes): `SdkJmpRec` in `sdklift.h` holds one and is shared between
`sdklift_gen.c` (built here) and `sdklift_rt.c` (built against glibc), and the comparison checks its size too.
