// Starting the game. With build/libb3host.so the game runs in this process and takes over the launcher's window
// (runtime/display.c); when it ends, the launcher starts again in a new window. Without it, build/b3-game runs as a
// child while the launcher's window is hidden.
#pragma once
#include "config.h"
#include <SDL3/SDL.h>
#include <string>

struct GameStart {
	std::string error;           // why it cannot start (nothing built yet, ...)
	bool in_process = false;     // libb3host.so is loaded: call run_in_process() once the launcher has let go of the window
	void* host = nullptr;
};

// Loads libb3host.so when there is one (its constructors bind game.so), and prepares the game's environment and
// working folder. Nothing is shown or released yet.
GameStart prepare_game(const Paths& paths);

// Hands the window to the game and runs it; does not come back (the launcher starts itself again when the game ends,
// unless the player closed the window).
[[noreturn]] void run_in_process(GameStart& start, const Paths& paths, SDL_Window* window);

// The fallback: b3-game as a child process, waited for while the window is hidden. Returns its exit code.
int run_child(const Paths& paths, SDL_Window* window);
