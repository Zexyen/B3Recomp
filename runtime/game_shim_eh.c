/* Shim of the game DLL generated WITH landing-pad resume entries (recomp_eh.py; build with RECOMP_SHIM=game_shim_eh.c).
 * Everything of game_shim.c, plus what the exception unwinder needs from main: the resume table, and the host's
 * nro_catch_call for the functions that have a catch handler. The host finds game_eh_bind with GetProcAddress
 * (nncxxrt.c), so an exe works with either kind of DLL. */
#include "game_shim.c"

typedef struct { uint64_t lp; void (*fn)(Ctx *, uint64_t); } NroResume;
extern const NroResume resume_table_main[];
extern const int resume_count_main;

static void (*host_catch_call)(Ctx *, void (*)(Ctx *, uint64_t));
void nro_catch_call(Ctx *c, void (*body)(Ctx *, uint64_t)) { host_catch_call(c, body); }

__declspec(dllexport) void game_eh_bind(void (*catch_call)(Ctx *, void (*)(Ctx *, uint64_t)), const NroResume **table, int *count) {
    host_catch_call = catch_call;
    *table = resume_table_main;
    *count = resume_count_main;
}

#include <stdio.h>
#include <stdlib.h>
#ifndef _WIN32
#include <time.h>
#endif
/* [b3recomp] Host versions of game functions (lifter/recomp_eh.py HOST_OVERRIDES: their direct calls come here).
 * f_7100b382c0(value w0, uint32 *even x1, uint32 *odd x2): even = the even bits of the value packed into 16, odd = the odd
 * bits; returns odd in x0 (the original: three bit reversals and a 32-step loop of read-modify-writes, about 1,300
 * instructions). Outputs that overlap go to the lifted original (its order of stores decides there).
 * SSBU_HOSTFN_CHECK=1 (test): on the first call, 200,000 values through both, outputs and x0 compared. */
void f_7100b382c0(Ctx *c);
static uint32_t even_bits16(uint32_t v) {
    v &= 0x55555555u; v = (v | v >> 1) & 0x33333333u; v = (v | v >> 2) & 0x0F0F0F0Fu; v = (v | v >> 4) & 0x00FF00FFu;
    return (v | v >> 8) & 0x0000FFFFu;
}
static void morton_split32(Ctx *c) {
    uint64_t a = c->x[1], b = c->x[2];
    uint32_t v = (uint32_t)c->x[0], e = even_bits16(v), o = even_bits16(v >> 1);
    memcpy((void *)(uintptr_t)a, &e, 4); memcpy((void *)(uintptr_t)b, &o, 4);
    c->x[0] = o; c->x[8] = 32; c->x[9] = 0x80000000u;
}
static void morton_check(Ctx *c) {
    static uint32_t out[4];
    uint64_t s = 0x9E3779B97F4A7C15ull;
    long bad = 0;
    for (int i = 0; i < 200000; i++) {
        Ctx t = *c, u = *c;
        s ^= s << 13; s ^= s >> 7; s ^= s << 17;
        uint32_t v = i < 64 ? (i < 32 ? 1u << i : ~(1u << (i - 32))) : (uint32_t)s;
        t.x[0] = u.x[0] = v; t.x[1] = u.x[1] = (uint64_t)(uintptr_t)&out[0]; t.x[2] = u.x[2] = (uint64_t)(uintptr_t)&out[1];
        out[0] = out[1] = 0xdeadbeef; morton_split32(&t); uint32_t e = out[0], o = out[1];
        out[0] = out[1] = 0xdeadbeef; f_7100b382c0(&u);
        if (e != out[0] || o != out[1] || t.x[0] != u.x[0]) { if (bad++ < 5) printf("hostfn: TEST: morton_split32(%08x): host %08x %08x x0 %llx, game %08x %08x x0 %llx\n", v, e, o, (unsigned long long)t.x[0], out[0], out[1], (unsigned long long)u.x[0]); }
    }
#ifndef _WIN32
    {   /* the time of each, on this thread (Linux only: clock_gettime) */
        struct timespec t0, t1, t2;
        Ctx t = *c; t.x[1] = (uint64_t)(uintptr_t)&out[0]; t.x[2] = (uint64_t)(uintptr_t)&out[1];
        clock_gettime(CLOCK_MONOTONIC, &t0);
        for (int i = 0; i < 100000; i++) { t.x[0] = (uint32_t)(i * 2654435761u); morton_split32(&t); }
        clock_gettime(CLOCK_MONOTONIC, &t1);
        for (int i = 0; i < 100000; i++) { t.x[0] = (uint32_t)(i * 2654435761u); t.x[31] = c->x[31]; f_7100b382c0(&t); }
        clock_gettime(CLOCK_MONOTONIC, &t2);
        printf("hostfn: TEST: morton_split32 %.1f ns a call, the lifted original %.1f ns\n", ((t1.tv_sec - t0.tv_sec) * 1e9 + (t1.tv_nsec - t0.tv_nsec)) / 1e5,
               ((t2.tv_sec - t1.tv_sec) * 1e9 + (t2.tv_nsec - t1.tv_nsec)) / 1e5);
    }
#endif
    printf("hostfn: TEST: morton_split32 against the game's f_7100b382c0: 200000 values, %ld differ\n", bad);
}
void h_morton_split32(Ctx *c) {
    static int chk = -1;
    if (chk < 0) { chk = 0; if (getenv("SSBU_HOSTFN_CHECK")) morton_check(c); }
    uint64_t a = c->x[1], b = c->x[2];
    if (a < b + 4 && b < a + 4) { f_7100b382c0(c); return; }
    morton_split32(c);
}
