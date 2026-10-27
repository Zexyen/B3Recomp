// The launcher: one SDL window, RmlUi screens on SDL's GPU API (ui/launcher.rml), the build (BuildRunner) and the start
// of the game (game_host). Screens: title menu, first run (the dump), building, mods, settings.
#pragma once
#include "build_runner.h"
#include "config.h"
#include "RmlUi_Platform_SDL.h"
#include "ui_render.h"
#include <RmlUi/Core.h>
#include <SDL3/SDL.h>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

struct CheckRow { Rml::String text, right, state; };
struct StepRow { Rml::String num, name, note, right, state; };
struct ModRow { Rml::String name, note, kind; bool on = true; };
struct SettingRow { Rml::String name, note, value; };

class App {
public:
	bool init(int argc, char** argv);
	int run();

private:
	Paths paths;
	SDL_Window* window = nullptr;
	SDL_GPUDevice* gpu = nullptr;
	std::unique_ptr<UiRender> render;
	std::unique_ptr<SystemInterface_SDL> system;
	TextInputMethodEditor_SDL ime;
	Rml::Context* ctx = nullptr;
	Rml::DataModelHandle model;
	std::vector<SDL_Gamepad*> pads;
	std::vector<std::vector<Rml::byte>> font_data;

	// what the screens show (the data model "wt")
	Rml::String screen = "title";
	bool built = false, status_bad = false;
	Rml::String status_title, status_line, footer_info;
	Rml::String dump;
	bool checking = false, verified = false;
	std::vector<CheckRow> checks;
	bool building = false;
	float progress = 0;
	int percent = 0;
	Rml::String phase, build_aside, build_error;
	std::vector<StepRow> steps;
	std::vector<Rml::String> log;
	std::vector<ModRow> mods;
	std::vector<ModEntry> mod_entries;
	Rml::String mods_summary, mods_error;
	bool mods_rebuild = false;
	std::vector<SettingRow> settings;

	// work for the main loop
	BuildRunner runner;
	enum class Job { none, verify, build } job = Job::none;
	bool running = true, want_play = false, want_focus = true;
	int returned_code = -1;            // --returned <code>: the game just ended
	std::mutex picked_mutex;
	std::string picked_folder;         // from the folder dialog (another thread)
	bool picked = false;

	bool create_window();
	bool create_ui();
	void bind_model();
	void handle(SDL_Event& ev);
	bool key(Rml::Input::KeyIdentifier k);
	void back();
	void tick();
	void draw();
	void rescale();
	void focus_first();

	void go(const Rml::String& to);
	void refresh_title();
	void refresh_build();
	void refresh_mods();
	void refresh_settings();
	void step_setting(int index, int dir);
	void toggle_mod(int index);
	void check_dump();
	void start_build();
	void browse();
	void play();
	void release_window();
};
