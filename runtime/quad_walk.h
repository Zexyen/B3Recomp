/* The game's quadtree walk 71007e1fe4 over two byte maps: a leaf is 1 when the maps differ there, 0 when they agree or it is
 * outside the map; a node returns 1 when all four children did, and otherwise draws a quad for each child that did. A node
 * whose leaves are all 0 or all 1 draws nothing, here or below, and writes only its stack, so its result can be read off
 * the maps. Lifted, that took 47 ms a walk. w, h: the largest x and y (both inclusive); rows are w apart.
 * Returns 0, 1, or -1 when the leaves of the n x n node at x0, y0 are mixed. */
#ifndef QUAD_WALK_H
#define QUAD_WALK_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>
static inline int quad_state(const uint8_t *a, const uint8_t *b, uint32_t w, uint32_t h, uint32_t x0, uint32_t y0, uint32_t n) {
    uint32_t x1 = x0 + n - 1, y1 = y0 + n - 1;
    int clipped = x1 > w || y1 > h;
    if (x1 > w) x1 = w;
    if (y1 > h) y1 = h;
    if (x0 > x1 || y0 > y1) return 0;
    size_t len = x1 - x0 + 1, at = (size_t)x0 + (size_t)y0 * w;
    int s = a[at] != b[at];
    if (s && clipped) return -1;
    for (uint32_t y = y0; y <= y1; y++, at += w) {
        if (!s) { if (memcmp(a + at, b + at, len)) return -1; }
        else for (size_t i = 0; i < len; i++) if (a[at + i] == b[at + i]) return -1;
    }
    return s;
}
#endif
