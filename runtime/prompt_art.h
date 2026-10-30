/* The pictures of the button prompts, drawn from plain shapes on a CELL x CELL cell, SS x SS subsamples per texel: 1 inside.
 * The game's atlas (prompts.c) and the launcher's footer (launcher/src/ui_render.cpp) use them. C and C++. */
#pragma once
#include <math.h>
#include <stdint.h>
#include <string.h>

#define CELL 40
#define SS 4                                /* subsamples per texel, per axis */
#define S (CELL * SS)

/* ---- shapes, in cell coordinates; v = 1 paints, 0 cuts */
static void disc(uint8_t *s, float cx, float cy, float r, int v) {
    for (int j = 0; j < S; j++) for (int i = 0; i < S; i++) {
        float x = (i + .5f) / SS - cx, y = (j + .5f) / SS - cy;
        if (x * x + y * y <= r * r) s[j * S + i] = (uint8_t)v;
    }
}
static void seg(uint8_t *s, float x0, float y0, float x1, float y1, float w, int v) {
    float dx = x1 - x0, dy = y1 - y0, len2 = dx * dx + dy * dy, r2 = w * w / 4;
    for (int j = 0; j < S; j++) for (int i = 0; i < S; i++) {
        float x = (i + .5f) / SS - x0, y = (j + .5f) / SS - y0, t = len2 > 0 ? (x * dx + y * dy) / len2 : 0;
        t = t < 0 ? 0 : t > 1 ? 1 : t;
        x -= t * dx; y -= t * dy;
        if (x * x + y * y <= r2) s[j * S + i] = (uint8_t)v;
    }
}
static void box(uint8_t *s, float x0, float y0, float x1, float y1, float r, int v) {     /* rounded */
    float cx = (x0 + x1) / 2, cy = (y0 + y1) / 2, hx = (x1 - x0) / 2 - r, hy = (y1 - y0) / 2 - r;
    for (int j = 0; j < S; j++) for (int i = 0; i < S; i++) {
        float x = fabsf((i + .5f) / SS - cx) - hx, y = fabsf((j + .5f) / SS - cy) - hy;
        x = x < 0 ? 0 : x; y = y < 0 ? 0 : y;
        if (x * x + y * y <= r * r) s[j * S + i] = (uint8_t)v;
    }
}
/* Strokes of a letter, 4 wide and 6 high, y down: pairs of hex digits (x, y) in half units; a space between pairs joins them,
 * '|' lifts the pen */
static const char *letter(char c) {
    static const char *const az[26] = {
        "0c 40 8c|28 68", "06 66 89 6c 0c 00 60 83 66", "83 60 20 03 09 2c 6c 89", "00 0c 5c 89 83 50 00", "80 00 0c 8c|06 66",
        "80 00 0c|06 66", "83 60 20 03 09 2c 6c 89 86 46", "00 0c|80 8c|06 86", "20 60|40 4c|2c 6c", "80 89 5c 2c 09",
        "00 0c|80 07|25 8c", "00 0c 8c", "0c 00 46 80 8c", "0c 00 8c 80", "20 60 83 89 6c 2c 09 03 20", "0c 00 60 83 66 06",
        "20 60 83 89 6c 2c 09 03 20|5a 8c", "0c 00 60 83 66 06|46 8c", "83 60 20 03 26 66 89 6c 2c 09", "00 80|40 4c",
        "00 09 2c 6c 89 80", "00 4c 80", "00 2c 46 6c 80", "00 8c|80 0c", "00 46 80|46 4c", "00 80 0c 8c"};
    static const char *const digit[10] = {
        "20 60 83 89 6c 2c 09 03 20|5a 33", "22 50 5c", "03 20 60 83 0c 8c", "03 20 60 83 66 89 6c 2c 09|36 66", "60 08 88|60 6c",
        "80 10 05 65 88 6c 2c 09", "70 20 03 09 2c 6c 89 86 66 26 07", "00 80 3c", "20 60 83 66 26 03 20|26 09 2c 6c 89 66",
        "86 26 03 20 60 83 89 6c 1c"};
    return c >= 'A' && c <= 'Z' ? az[c - 'A'] : c >= '0' && c <= '9' ? digit[c - '0'] : "";
}
static int hex(char c) { return c <= '9' ? c - '0' : c - 'a' + 10; }
static void text(uint8_t *s, const char *t) {               /* one to four letters, cut out at the centre */
    static const float size[5][3] = {{0}, {2.1f, 2.4f, 0}, {1.45f, 1.8f, 2.6f}, {1.05f, 1.5f, 1.7f}, {.95f, 1.4f, 1.3f}};   /* scale, stroke, gap */
    int n = (int)strlen(t);
    float k = size[n][0], w = size[n][1], gap = size[n][2], x0 = (CELL - n * 4 * k - (n - 1) * gap) / 2, y0 = (CELL - 6 * k) / 2;
    for (int i = 0; i < n; i++) {
        float ox = x0 + i * (4 * k + gap);
        for (const char *p = letter(t[i]); p[0] && p[2] && p[3]; p += 3)
            if (p[2] == ' ') seg(s, ox + hex(p[0]) * k / 2, y0 + hex(p[1]) * k / 2, ox + hex(p[3]) * k / 2, y0 + hex(p[4]) * k / 2, w, 0);
    }
}

/* the disc of the game's own face buttons */
#define DISC(s) disc(s, 20, 20, 10.5f, 1)
static void letter_disc(uint8_t *s, const char *t) { DISC(s); text(s, t); }
static void ps_circle(uint8_t *s) { DISC(s); disc(s, 20, 20, 6.4f, 0); disc(s, 20, 20, 4.3f, 1); }
static void ps_cross(uint8_t *s) { DISC(s); seg(s, 15.6f, 15.6f, 24.4f, 24.4f, 2.2f, 0); seg(s, 24.4f, 15.6f, 15.6f, 24.4f, 2.2f, 0); }
static void ps_square(uint8_t *s) {
    DISC(s);
    seg(s, 15, 15, 25, 15, 2.1f, 0); seg(s, 25, 15, 25, 25, 2.1f, 0); seg(s, 25, 25, 15, 25, 2.1f, 0); seg(s, 15, 25, 15, 15, 2.1f, 0);
}
static void ps_triangle(uint8_t *s) {
    DISC(s);
    seg(s, 20, 13.8f, 26, 24, 2.1f, 0); seg(s, 26, 24, 14, 24, 2.1f, 0); seg(s, 14, 24, 20, 13.8f, 2.1f, 0);
}
static void bars_across(uint8_t *s) {                       /* Options / Menu */
    DISC(s);
    for (int i = -1; i <= 1; i++) seg(s, 15.2f, 20 + 3.8f * i, 24.8f, 20 + 3.8f * i, 1.9f, 0);
}
static void bars_up(uint8_t *s) {                           /* Create */
    DISC(s);
    for (int i = -1; i <= 1; i++) seg(s, 20 + 3.8f * i, 15.2f, 20 + 3.8f * i, 24.8f, 1.9f, 0);
}
static void view(uint8_t *s) {                              /* View: two windows */
    DISC(s);
    box(s, 17.5f, 13.5f, 26.5f, 22.5f, 1, 0); box(s, 19.3f, 15.3f, 24.7f, 20.7f, .5f, 1);
    box(s, 13, 17.5f, 22.5f, 27, 1, 0); box(s, 14.9f, 19.4f, 20.6f, 25.1f, .5f, 1);
}
static void bumper(uint8_t *s, const char *t) { box(s, 7, 11.5f, 33, 28.5f, 3, 1); text(s, t); }
static void trigger(uint8_t *s, const char *t) { box(s, 8, 9, 32, 31, 7, 1); box(s, 8, 20, 32, 31, 2, 1); text(s, t); }
static void keycap(uint8_t *s, const char *t) {
    box(s, strlen(t) > 3 ? 4 : 7, 9, strlen(t) > 3 ? 36 : 33, 31, 4, 1);
    text(s, t);
}
static void mouse(uint8_t *s, int button) {                 /* 0 none (it moves), 1 left, 2 right, 3 wheel */
    box(s, 12.5f, 8, 27.5f, 32, 7.5f, 1); box(s, 14.2f, 9.7f, 25.8f, 30.3f, 5.8f, 0);
    seg(s, 13, 19, 27, 19, 1.5f, 1);
    if (button == 3) seg(s, 20, 11, 20, 16.5f, 3, 1);
    else { seg(s, 20, 9, 20, 19, 1.5f, 1); if (button) box(s, button == 1 ? 14.2f : 20, 9.7f, button == 1 ? 20 : 25.8f, 19, 2, 1); }
}
