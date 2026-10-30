/* The step of a fixed-step frame (game_shim_eh.c h_frame_clock), in 30 Hz frames (#47). A cutscene's long sounds run on real
 * time, but the timeline starts them and its other sounds, so a scene that lags or rushes takes them along. On time the step
 * is even, so jitter in the measured length does not move the picture. *owed is how far the scene is behind real time: up to
 * one even step stays owed, past that an eighth of the rest goes each frame, at most 2.0 a frame, so a stall is paid without
 * a burst and a machine below the frame rate still keeps real time (down to 15 fps; slower, it lags by at most 90 frames). */
#ifndef SCENE_STEP_H
#define SCENE_STEP_H
static float scene_step(float *owed, float due, float even) {
    float o = *owed + due - even, k = even;
    if (o > 90.0f) o = 90.0f;                      /* a window held for minutes must not fast-forward the scene for minutes */
    if (o > even) k += (o - even) / 8.0f; else if (o < -even) k += (o + even) / 8.0f;
    if (k > 2.0f) k = 2.0f;
    *owed = o + even - k;
    return k;
}
#endif
