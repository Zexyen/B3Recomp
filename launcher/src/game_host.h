// Starting the game. With build/libwitchtime.so the game runs in this process and takes over the launcher's window
// (runtime/display.c); when it ends, the launcher starts again in a new window. Without it, build/witch-time-game runs as a
// child while the launcher's window is hidden. On Windows it always runs as a child (witch-time-game.exe makes its own
// window). Either way its output goes to build/run/witch-time-game.log.
#pragma once
#include "config.h"
#include <SDL3/SDL.h>
#include <string>

struct GameStart {
	std::string error;           // why it cannot start (nothing built yet, ...)
	bool in_process = false;     // libwitchtime.so is loaded: call run_in_process() once the launcher has let go of the window
	void* host = nullptr;
};

// Loads libwitchtime.so when there is one (its constructors bind game.so), and prepares the game's environment and
// working folder. Nothing is shown or released yet.
GameStart prepare_game(const Paths& paths);

// Hands the window to the game and runs it; does not come back (the launcher starts itself again when the game ends if
// `relaunch`, unless the player closed the window).
[[noreturn]] void run_in_process(GameStart& start, const Paths& paths, SDL_Window* window, bool relaunch);

// The fallback: witch-time-game as a child process, waited for while the window is hidden. Returns its exit code.
int run_child(const Paths& paths, SDL_Window* window);
