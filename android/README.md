# Witch Time for Android

In progress: the app builds and the game's code builds for it, but it has not run on a phone yet. AArch64 (arm64-v8a),
Android 10 or newer, a GPU with Vulkan 1.1.

The app (`WitchTime-<version>-android.apk`) holds the runtime and the libraries it uses, nothing from the game. Your PC
builds the game library for it from your dump, as it does for itself:

1. Set up Witch Time on your PC as usual (`wtbuild setup --dump <dump>`), and unpack
   `WitchTime-<version>-android-prebuilt.zip` into the Witch Time folder (it adds `prebuilt-android/`).
2. Build the Android game library: `build/wtbuild compile --target android`, then `build/wtbuild link --target android`.
   It is written to `build/android/game.so`; the objects are kept in `build/obj_android`.
3. Install the app once and start it, so that Android makes its folder, `Android/data/com.zexyen.witchtime/files` on
   the phone's shared storage. That folder is a Witch Time folder, as on the PC:
   - `targets/` of your PC's Witch Time folder (`exefs/main.elf`, `exefs/sdk.elf`, `romfs/`)
   - `build/game.so`: your PC's `build/android/game.so`
   - `config/` (optional: `graphics.ini`, `controllers.ini`)
4. Start the app. It copies `build/game.so` into its own storage (code cannot run from the shared storage) and starts
   the game; the runtime's log is `build/run/witch-time-game.log`, saves go to `build/userdata`.

## Controls

A controller works as on a PC: one paired over Bluetooth or USB, or a handheld's built-in controller. Without one the
app shows touch controls over the game: a stick where the left thumb lands, a drag on the right half for the camera, and
the Switch's buttons where a Switch has them (RS is the right stick press, lock-on). `touch_controls` in
`config/controllers.ini` decides: `auto` (the default) shows them only while no controller is connected and hides them
when one connects, `on` always, `off` never. The app reads it when it comes back to the front. The touch controls are a
controller of their own to the game ("Witch Time touch controls"), so `controllers.ini` can remap them like any pad.

Uninstalling asks whether to keep the app's data: keep it, or the folder above, with the game data, is deleted.

## Building the app

```
SDL_TAG=... FFMPEG_TAG=... GLSLANG_TAG=... VULKAN_HEADERS_TAG=... OPUS_TAG=... android/build_deps.sh <ndk r29> <deps>
LLVM_ROOT=<ndk>/toolchains/llvm/prebuilt/linux-x86_64 CPATH=... WT_GLSLANG_LIBS=... \
    build/wtbuild runtime --target android --out prebuilt-android
gradle -p android assembleRelease -Pwitchtime.deps=<deps> -Pwitchtime.prebuilt=$PWD/prebuilt-android
```

The tags, `CPATH` and `WT_GLSLANG_LIBS` are those of job `android` in `.github/workflows/release.yml`, which is the
reference. The runtime build needs SDL3, FFmpeg and libopus copied into `prebuilt-android/` first. The APK is signed with the
debug key for now, so a newer build may need the old one uninstalled first (keep the data when asked).

`WitchTimeActivity.java` is SDL's activity, which loads `libSDL3.so`, then `game.so`, then `libwitchtime.so`, whose
`SDL_main` (`runtime/wt_main.c`) sets up the paths and runs the game. `TouchControls.java` is the touch layer (dusklight's
touch controls were the reference); it registers with SDL through `org/libsdl/app/TouchPadBridge.java`, since SDL's
controller natives are package-private.
