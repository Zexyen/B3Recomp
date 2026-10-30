/* runtime/scene_step.h: steady frames take the even step, a hitch is paid gently, a stall in full, a slow machine keeps real
 * time.   clang -O2 -Wall -I runtime tests/scene_step_test.c -o build/scene_step_test */
#include <math.h>
#include <stdio.h>
#include "scene_step.h"

static int bad;
static void check(const char *what, int ok) { if (!ok) { printf("FAILED: %s\n", what); bad = 1; } }

int main(void) {
    float owed = 0.0f, k, top = 0.0f;
    for (int i = 0; i < 1000; i++) check("on time: the even step", scene_step(&owed, 1.0f, 1.0f) == 1.0f);
    for (int i = 0; i < 1000; i++) check("jitter within a frame: the even step", scene_step(&owed, i & 1 ? 1.2f : 0.8f, 1.0f) == 1.0f);
    owed = 0.0f;
    k = scene_step(&owed, 3.0f, 1.0f);                     /* a 100 ms frame at 30 fps */
    check("a hitch is no burst", k < 1.2f);
    owed = 0.0f;
    k = scene_step(&owed, 9.0f, 1.0f);                     /* a 300 ms stall */
    for (int i = 0; i < 200; i++) { k = scene_step(&owed, 1.0f, 1.0f); if (k > top) top = k; }
    check("a stall is paid down to a frame", owed < 1.01f && fabsf(k - 1.0f) < 0.001f && top < 2.0f);
    owed = 0.0f;
    for (int i = 0; i < 1000; i++) k = scene_step(&owed, 1.5f, 1.0f);   /* 20 fps in a 30 fps scene */
    check("a slow machine keeps real time", fabsf(k - 1.5f) < 0.001f && owed < 6.0f);
    owed = 0.0f;
    for (int i = 0; i < 100000; i++) k = scene_step(&owed, 100.0f, 1.0f);
    check("the debt is capped", owed <= 90.0f && k <= 2.0f);
    if (!bad) printf("scene_step: ok\n");
    return bad;
}
