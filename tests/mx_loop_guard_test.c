/* runtime/mx_translate.c, [issue 26]: a step back in a graphics stage is counted (the stage ends after MX_LOOP_MAX of them),
 * a compute stage is left alone; MxIn.loopdebug sets the limit. A hand-made stage: NOP, @P0 BRA back to the NOP, EXIT.
 *   clang -O2 -Wall -I runtime tests/mx_loop_guard_test.c runtime/mx_translate.c -o build/mx_loop_guard_test */
#include <stdio.h>
#include <string.h>
#include "mx_translate.h"

static char *translate(int stage, const uint8_t *code, int len, unsigned loopdebug) {
    MxIn in; MxOut out;
    memset(&in, 0, sizeof in); memset(&out, 0, sizeof out);
    in.stage = stage; in.code = code; in.code_len = len; in.target = 1; in.loopdebug = loopdebug;
    mx_translate(&in, &out);
    if (!out.glsl) printf("stage %d not translated: %s %s\n", stage, out.op, out.why);
    return out.glsl;
}

int main(void) {
    uint8_t code[0x50 + 32];                       /* a zeroed program header, then one group of three instructions */
    uint64_t w[4] = {0,
        0x50b0000000000f00ull | 7ull << 16,                            /* NOP */
        0xe24000000000000full | (uint64_t)(-16 & 0xffffff) << 20,      /* @P0 BRA to the NOP */
        0xe30000000000000full | 7ull << 16};                           /* EXIT */
    memset(code, 0, sizeof code);
    memcpy(code + 0x50, w, sizeof w);
    char *v = translate(0, code, sizeof code, 0), *c = translate(5, code + 0x50, sizeof w, 0), *d = translate(0, code, sizeof code, 10);
    int ok = v && c && d && strstr(v, "uint mx_back = 0u;") && strstr(v, "if (++mx_back > 4096u)") && !strstr(c, "mx_back") && strstr(d, "if (++mx_back > 10u)");
    printf("mx_loop_guard_test: %s\n", ok ? "ok" : "FAILED");
    return !ok;
}
