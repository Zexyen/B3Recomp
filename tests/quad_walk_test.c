/* runtime/quad_walk.h against the walk's leaf rule, leaf by leaf, on random maps with uniform patches and on nodes past the
 * edges.   clang -O2 -Wall -I runtime tests/quad_walk_test.c -o build/quad_walk_test */
#include <stdio.h>
#include <stdlib.h>
#include "quad_walk.h"

#define MAX 80
static uint8_t a[MAX * MAX + MAX], b[MAX * MAX + MAX];
static int leaf(uint32_t w, uint32_t h, uint32_t x, uint32_t y) { return x > w || y > h ? 0 : a[x + y * w] != b[x + y * w]; }

int main(void) {
    long seen[3] = {0, 0, 0};
    for (int run = 0; run < 200000; run++) {
        uint32_t w = 1 + rand() % (MAX - 1), h = 1 + rand() % (MAX - 1), n = 1u << (rand() % 7), x0 = (rand() % (MAX + 8)) / n * n, y0 = (rand() % (MAX + 8)) / n * n;
        memset(a, 7, sizeof a); memset(b, 7, sizeof b);
        int fill = rand() % 3;
        if (fill == 1) memset(b, 9, sizeof b);
        for (int k = rand() % 3 * (rand() % 4); k > 0; k--) { uint32_t i = rand() % (MAX * MAX); b[i] = a[i] ^ (uint8_t)(rand() & 1); }
        int zero = 1, one = 1;
        for (uint32_t y = y0; y < y0 + n; y++)
            for (uint32_t x = x0; x < x0 + n; x++) { if (leaf(w, h, x, y)) zero = 0; else one = 0; }
        int want = zero ? 0 : one ? 1 : -1, got = quad_state(a, b, w, h, x0, y0, n);
        seen[want + 1]++;
        if (got != want) { printf("FAILED: map %u x %u, node %u at %u, %u: %d, should be %d\n", w, h, n, x0, y0, got, want); return 1; }
    }
    if (!seen[0] || !seen[1] || !seen[2]) { printf("FAILED: a state never came up: %ld %ld %ld\n", seen[0], seen[1], seen[2]); return 1; }
    printf("quad_walk: ok (%ld mixed, %ld all 0, %ld all 1)\n", seen[0], seen[1], seen[2]);
    return 0;
}
