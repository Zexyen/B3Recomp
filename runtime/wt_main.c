/* witch-time-game: the runtime is libwitchtime.so beside this program (boot.c wt_host_main), so the whole of it can be
 * prebuilt; the launcher starts this when it cannot load the library itself. */
int wt_host_main(int argc, char **argv);
#ifdef __ANDROID__
/* [witchtime] Android: this file is part of libwitchtime.so, and the app's SDLActivity calls SDL_main (SDL_main.h renames
 * main). The app's external storage folder (Android/data/<app>/files on the shared storage) is a Witch Time folder, as
 * the launcher runs the game from on a PC (launcher/src/game_host.cpp): targets/exefs (main.elf, sdk.elf), targets/romfs,
 * config/, and build/, whose witch-time-game the runtime takes for its program (plat_posix.c GetModuleFileNameA). */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
int main(int argc, char **argv) {
    (void)argc; (void)argv;
    static char exe[1024], elf[1024], rom[1024], run[1024];
    const char *dir = SDL_GetAndroidExternalStoragePath();
    if (!dir) { SDL_Log("witch-time: no external storage folder: %s", SDL_GetError()); return 2; }
    snprintf(exe, sizeof exe, "%s/build/witch-time-game", dir);
    snprintf(elf, sizeof elf, "%s/targets/exefs/main.elf", dir);
    snprintf(rom, sizeof rom, "%s/targets/romfs", dir);
    snprintf(run, sizeof run, "%s/build", dir);
    mkdir(run, 0755);
    snprintf(run, sizeof run, "%s/build/run", dir);
    mkdir(run, 0755);
    setenv("SSBU_ROMFS", rom, 0);
    setenv("SSBU_TITLE", "Witch Time", 0);
    setenv("SSBU_VK_MISSING", "warn", 0);
    if (chdir(run)) { SDL_Log("witch-time: %s: cannot open it", run); return 2; }
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");   /* the window is windowed and resizable (display.c) */
    freopen("witch-time-game.log", "w", stdout);   /* the runtime reports on stdout and stderr: logcat shows neither */
    setvbuf(stdout, NULL, _IOLBF, 0);
    dup2(fileno(stdout), 2);
    char *args[] = { exe, elf, NULL };
    return wt_host_main(2, args);
}
#else
int main(int argc, char **argv) { return wt_host_main(argc, argv); }
#endif
