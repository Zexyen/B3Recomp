#include "app.h"
#include "game_host.h"
#include "platform.h"
#include "ui_render.h"
#include "update.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <sys/stat.h>

namespace {

// the layout size of every screen, in dp; the window scales it (rescale)
constexpr float LAYOUT_W = 1280, LAYOUT_H = 800;

// the build's steps as five rows a player follows, and how much of the whole build each step is (about)
struct Group { const char *num, *name, *note; std::vector<std::string> steps; };
const std::vector<Group>& groups()
{
	static const std::vector<Group> g = {
		{"I", "Check your dump", "Tools, version and every data file", {"deps", "verify"}},
		{"II", "Unpack and apply mods", "Code files and your code mods", {"convert", "mods"}},
		{"III", "Translate the code", "Switch code to portable C", {"tables", "lift"}},
		{"IV", "Compile", "The longest step", {"compile"}},
		{"V", "Link and finish", "Menu text from your copy, self-test", {"link", "text", "check"}},
	};
	return g;
}
float weight(const std::string& step)
{
	static const std::vector<std::pair<std::string, float>> w = {{"deps", 1}, {"verify", 6}, {"convert", 1}, {"mods", 1}, {"tables", 4},
		{"lift", 22}, {"compile", 55}, {"link", 6}, {"text", 2}, {"check", 2}};
	for (auto& [s, v] : w)
		if (s == step)
			return v / 100.f;
	return 0;
}
int step_index(const std::string& step)
{
	const auto& s = BuildRunner::steps();
	auto it = std::find(s.begin(), s.end(), step);
	return it == s.end() ? -1 : int(it - s.begin());
}
const char* phase_of(const std::string& step)
{
	static const std::vector<std::pair<std::string, const char*>> p = {{"deps", "checking tools"}, {"verify", "checking your dump"},
		{"convert", "unpacking"}, {"mods", "applying mods"}, {"tables", "reading the SDK"}, {"lift", "translating"},
		{"compile", "compiling"}, {"link", "linking"}, {"text", "making menu text"}, {"check", "self-testing"}};
	for (auto& [s, v] : p)
		if (s == step)
			return v;
	return "working";
}

struct SettingDef {
	const char *key, *name, *note;
	std::vector<std::pair<const char*, const char*>> options;   // value in graphics.ini, what the screen shows
};
const std::vector<SettingDef>& setting_defs()
{
	static const std::vector<SettingDef> d = {
		{"resolution_scale", "Internal resolution", "Takes effect at the next start",
			{{"1.0", "1.0× · 900p"}, {"1.2", "1.2× · 1080p"}, {"1.4", "1.4× · 1260p"}, {"1.6", "1.6× · 1440p"}, {"2.0", "2.0× · 1800p"}, {"2.4", "2.4× · 4K"},
				{"0.4", "0.4× · 360p"}, {"0.6", "0.6× · 540p"}, {"0.8", "0.8× · 720p"}}},   // after 2.4: the first option is the default, and left from 1.0 goes down
		{"cutscene_fps", "Cutscene frame rate", "Menus and cutscenes; movies keep their own", {{"30", "30"}, {"60", "60"}}},
		{"window", "Display mode", "How the game uses the window", {{"windowed", "Window"}, {"borderless", "Borderless"}, {"fullscreen", "Fullscreen"}}},
		{"vsync", "Vsync", "Auto measures your display at start", {{"auto", "Auto"}, {"display", "Display"}, {"clock", "Clock"}, {"off", "Off"}}},
		{"anisotropic_filtering", "Anisotropic filtering", "Sharper textures at an angle", {{"game", "Game"}, {"2", "2×"}, {"4", "4×"}, {"8", "8×"}, {"16", "16×"}}},
		{"aspect", "Aspect", "Keep the 16:9 picture, or fill the window", {{"keep", "Keep 16:9"}, {"stretch", "Stretch"}}},
	};
	return d;
}
bool same_value(const std::string& a, const std::string& b)
{
	char *ea = nullptr, *eb = nullptr;
	double x = std::strtod(a.c_str(), &ea), y = std::strtod(b.c_str(), &eb);
	if (ea != a.c_str() && !*ea && eb != b.c_str() && !*eb)
		return std::fabs(x - y) < 1e-6;
	return SDL_strcasecmp(a.c_str(), b.c_str()) == 0;
}
int option_index(const SettingDef& d, const std::string& value)
{
	for (size_t i = 0; i < d.options.size(); i++)
		if (same_value(d.options[i].first, value))
			return int(i);
	return -1;
}

std::string minutes(double s)
{
	char b[32];
	std::snprintf(b, sizeof b, s < 60 ? "%.0f s" : "%.0f min", s < 60 ? s : s / 60);
	return b;
}

} // namespace

// ---------------------------------------------------------------------------------------------------- start and loop
bool App::init(int argc, char** argv)
{
	paths = Paths::find(argv[0]);
	for (int i = 1; i < argc; i++)
		if (!std::strcmp(argv[i], "--returned") && i + 1 < argc)
			returned_code = std::atoi(argv[i + 1]);
		else if (!std::strcmp(argv[i], "--play"))
			want_play = play_only = true;
		else if (!std::strcmp(argv[i], "--updated"))
			updated = true;
	SDL_SetAppMetadata("Witch Time", nullptr, "witchtime");
	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS))
	{
		std::fprintf(stderr, "witch-time: SDL cannot start: %s\n", SDL_GetError());
		return false;
	}
	if (!create_window() || !create_ui())
		return false;
	SetupState st = SetupState::read(paths.build);
	dump = st.dump;
	refresh_title();
	refresh_build();
	update::check(paths);
	if (const char* s = std::getenv("WITCHTIME_SCREEN"))    // test aid: start on another screen (setup, build, mods, settings)
		go(s);
	return true;
}

bool App::create_window()
{
	SDL_Rect usable{0, 0, 1920, 1080};
	SDL_GetDisplayUsableBounds(SDL_GetPrimaryDisplay(), &usable);
	float scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
	scale = scale > 0 ? scale : 1;
	int w = int(LAYOUT_W * scale), h = int(LAYOUT_H * scale);
	if (w > usable.w * 0.9f || h > usable.h * 0.9f)
	{
		float f = std::min(usable.w * 0.9f / w, usable.h * 0.9f / h);
		w = int(w * f);
		h = int(h * f);
	}
	// a Vulkan window: the game makes its own Vulkan surface on it after the launcher lets go (runtime/display.c)
	window = SDL_CreateWindow("Witch Time", w, h, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
	if (!window)
	{
		std::fprintf(stderr, "witch-time: no window: %s\n", SDL_GetError());
		return false;
	}
	SDL_SetWindowMinimumSize(window, 640, 400);
	gpu = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, false, nullptr);
	if (!gpu || !SDL_ClaimWindowForGPUDevice(gpu, window))
	{
		std::fprintf(stderr, "witch-time: no Vulkan device for the launcher's screens: %s\n", SDL_GetError());
		return false;
	}
	return true;
}

bool App::create_ui()
{
	render = std::make_unique<UiRender>(gpu, window);
	system = std::make_unique<SystemInterface_SDL>(window);
	Rml::SetSystemInterface(system.get());
	Rml::SetRenderInterface(render.get());
	Rml::SetTextInputHandler(&ime);
	Rml::Initialise();
	// by file, with the family named here: the files of one family do not all carry the same family name
	struct Font { const char *file, *family; Rml::Style::FontStyle style; int weight; };
	const Font fonts[] = {
		{"Cinzel-Medium.ttf", "Cinzel", Rml::Style::FontStyle::Normal, 500},
		{"Cinzel-SemiBold.ttf", "Cinzel", Rml::Style::FontStyle::Normal, 600},
		{"CormorantGaramond-Medium.ttf", "Cormorant Garamond", Rml::Style::FontStyle::Normal, 500},
		{"CormorantGaramond-MediumItalic.ttf", "Cormorant Garamond", Rml::Style::FontStyle::Italic, 500},
		{"BarlowSemiCondensed-Regular.ttf", "Barlow Semi Condensed", Rml::Style::FontStyle::Normal, 400},
		{"BarlowSemiCondensed-Medium.ttf", "Barlow Semi Condensed", Rml::Style::FontStyle::Normal, 500},
		{"BarlowSemiCondensed-SemiBold.ttf", "Barlow Semi Condensed", Rml::Style::FontStyle::Normal, 600},
	};
	for (const Font& f : fonts)
	{
		SDL_IOStream* io = SDL_IOFromFile((paths.ui + "/fonts/" + f.file).c_str(), "rb");
		size_t size = 0;
		void* bytes = io ? SDL_LoadFile_IO(io, &size, true) : nullptr;
		if (!bytes)
		{
			std::fprintf(stderr, "witch-time: font %s missing\n", f.file);
			continue;
		}
		font_data.emplace_back(static_cast<Rml::byte*>(bytes), static_cast<Rml::byte*>(bytes) + size);   // RmlUi keeps using it
		SDL_free(bytes);
		Rml::LoadFontFace({font_data.back().data(), font_data.back().size()}, f.family, f.style, Rml::Style::FontWeight(f.weight), f.weight == 400);
	}
	int pw = 0, ph = 0;
	SDL_GetWindowSizeInPixels(window, &pw, &ph);
	ctx = Rml::CreateContext("main", {pw, ph});
	if (!ctx)
		return false;
	bind_model();
	Rml::ElementDocument* doc = ctx->LoadDocument(paths.ui + "/launcher.rml");
	if (!doc)
	{
		std::fprintf(stderr, "witch-time: cannot load %s/launcher.rml\n", paths.ui.c_str());
		return false;
	}
	doc->Show();
	rescale();
	return true;
}

void App::bind_model()
{
	Rml::DataModelConstructor c = ctx->CreateDataModel("wt");
	if (auto h = c.RegisterStruct<CheckRow>())
	{
		h.RegisterMember("text", &CheckRow::text);
		h.RegisterMember("right", &CheckRow::right);
		h.RegisterMember("state", &CheckRow::state);
	}
	if (auto h = c.RegisterStruct<StepRow>())
	{
		h.RegisterMember("num", &StepRow::num);
		h.RegisterMember("name", &StepRow::name);
		h.RegisterMember("note", &StepRow::note);
		h.RegisterMember("right", &StepRow::right);
		h.RegisterMember("state", &StepRow::state);
	}
	if (auto h = c.RegisterStruct<ModRow>())
	{
		h.RegisterMember("name", &ModRow::name);
		h.RegisterMember("note", &ModRow::note);
		h.RegisterMember("kind", &ModRow::kind);
		h.RegisterMember("on", &ModRow::on);
	}
	if (auto h = c.RegisterStruct<SettingRow>())
	{
		h.RegisterMember("name", &SettingRow::name);
		h.RegisterMember("note", &SettingRow::note);
		h.RegisterMember("value", &SettingRow::value);
	}
	c.RegisterArray<std::vector<CheckRow>>();
	c.RegisterArray<std::vector<StepRow>>();
	c.RegisterArray<std::vector<ModRow>>();
	c.RegisterArray<std::vector<SettingRow>>();
	c.RegisterArray<std::vector<Rml::String>>();
	c.Bind("screen", &screen);
	c.Bind("built", &built);
	c.Bind("hour_deg", &hour_deg);
	c.Bind("minute_deg", &minute_deg);
	c.Bind("status_bad", &status_bad);
	c.Bind("status_title", &status_title);
	c.Bind("status_line", &status_line);
	c.Bind("footer_info", &footer_info);
	c.Bind("update_tag", &update_tag);
	c.Bind("dump", &dump);
	c.Bind("checking", &checking);
	c.Bind("verified", &verified);
	c.Bind("move_dump", &move_dump);
	c.Bind("dump_modified", &dump_modified);
	c.Bind("accept_modified", &accept_modified);
	c.Bind("checks", &checks);
	c.Bind("building", &building);
	c.Bind("progress", &progress);
	c.Bind("percent", &percent);
	c.Bind("phase", &phase);
	c.Bind("build_aside", &build_aside);
	c.Bind("build_error", &build_error);
	c.Bind("steps", &steps);
	c.Bind("log", &log);
	c.Bind("mods", &mods);
	c.Bind("mods_summary", &mods_summary);
	c.Bind("mods_error", &mods_error);
	c.Bind("mods_rebuild", &mods_rebuild);
	c.Bind("settings", &settings);
	c.BindFunc("pad", [this](Rml::Variant& v) { v = !pads.empty(); });    // footer hints: pad buttons, else keys
	auto arg_int = [](const Rml::VariantList& a, size_t i) { return i < a.size() ? a[i].Get<int>() : 0; };
	c.BindEventCallback("go", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a) {
		if (!a.empty())
			go(a[0].Get<Rml::String>());
	});
	c.BindEventCallback("play", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) { want_play = true; });
	c.BindEventCallback("update", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
		if (!runner.running())
			update::start(paths);
	});
	c.BindEventCallback("quit", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) { running = false; });
	c.BindEventCallback("browse", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) { browse(); });
	c.BindEventCallback("check_dump", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) { check_dump(); });
	c.BindEventCallback("toggle_move", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
		move_dump = !move_dump;
		model.DirtyVariable("move_dump");
	});
	c.BindEventCallback("toggle_accept_modified", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
		if (runner.running())
			return;
		accept_modified = !accept_modified;
		model.DirtyVariable("accept_modified");
		check_dump();    // the verdict depends on it
	});
	c.BindEventCallback("start_build", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) { start_build(); });
	c.BindEventCallback("stop_build", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) { runner.stop(); });
	c.BindEventCallback("open_mods", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
		SetupState st = SetupState::read(paths.build);
		std::string dir = st.mods_dir.empty() ? paths.mods : st.mods_dir;
		make_dir(dir);
		SDL_OpenURL(((dir[0] == '/' ? "file://" : "file:///") + dir).c_str());   // file:///C:/... on Windows
	});
	c.BindEventCallback("toggle_mod", [this, arg_int](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a) { toggle_mod(arg_int(a, 0)); });
	c.BindEventCallback("setting", [this, arg_int](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& a) {
		step_setting(arg_int(a, 0), arg_int(a, 1));
	});
	model = c.GetModelHandle();
}

int App::run()
{
	while (running)
	{
		SDL_Event ev;
		// while an animation plays RmlUi asks for the next frame at once; the swapchain's vsync paces it
		const int wait = ctx->GetNextUpdateDelay() <= 0 ? 1 : runner.running() ? 30 : 50;
		if (SDL_WaitEventTimeout(&ev, wait))
		{
			handle(ev);
			while (running && SDL_PollEvent(&ev))
				handle(ev);
		}
		tick();
		if (want_play)
		{
			want_play = false;
			play();
		}
		draw();
	}
	if (runner.running())
		runner.stop();                       // the build resumes from this step next time
	release_window();
	SDL_DestroyWindow(window);
	SDL_Quit();
	std::string tag, message;
	if (update::state(tag, message) == update::State::done)
		update::restart(paths);
	return 0;
}

void App::draw()
{
	ctx->Update();
	if (want_focus)
	{
		want_focus = false;
		focus_first();
		ctx->Update();
	}
	SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(gpu);
	if (!cmd)
		return;
	SDL_GPUTexture* target = nullptr;
	Uint32 w = 0, h = 0;
	if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmd, window, &target, &w, &h) || !target)
	{
		SDL_CancelGPUCommandBuffer(cmd);
		return;
	}
	SDL_GPUColorTargetInfo clear{};
	clear.texture = target;
	clear.clear_color = {10 / 255.f, 6 / 255.f, 16 / 255.f, 1};
	clear.load_op = SDL_GPU_LOADOP_CLEAR;
	clear.store_op = SDL_GPU_STOREOP_STORE;
	SDL_EndGPURenderPass(SDL_BeginGPURenderPass(cmd, &clear, 1, nullptr));
	render->BeginFrame(cmd, target, w, h);
	ctx->Render();
	render->EndFrame();
	SDL_SubmitGPUCommandBuffer(cmd);
}

void App::rescale()
{
	int pw = 0, ph = 0;
	SDL_GetWindowSizeInPixels(window, &pw, &ph);
	ctx->SetDimensions({pw, ph});
	// the screens are laid out for 1280 x 800 dp and grow and shrink with the window, as a game's menus do
	ctx->SetDensityIndependentPixelRatio(std::max(0.25f, std::min(pw / LAYOUT_W, ph / LAYOUT_H)));
}

void App::focus_first()
{
	Rml::ElementDocument* doc = ctx->GetDocument(0);
	if (!doc)
		return;
	Rml::ElementList list;
	doc->GetElementsByClassName(list, "autofocus");                // the other screens' are there too, hidden (data-if)
	for (Rml::Element* e : list)
		if (e->IsVisible(true) && e->Focus(true))
			return;
}

// ---------------------------------------------------------------------------------------------------- input
void App::handle(SDL_Event& ev)
{
	switch (ev.type)
	{
	case SDL_EVENT_QUIT: running = false; return;
	case SDL_EVENT_GAMEPAD_ADDED:
		if (SDL_Gamepad* p = SDL_OpenGamepad(ev.gdevice.which))
			pads.push_back(p);
		model.DirtyVariable("pad");
		refresh_title();
		return;
	case SDL_EVENT_GAMEPAD_REMOVED:
		for (auto it = pads.begin(); it != pads.end();)
			it = SDL_GetGamepadID(*it) == ev.gdevice.which ? (SDL_CloseGamepad(*it), pads.erase(it)) : it + 1;
		model.DirtyVariable("pad");
		refresh_title();
		return;
	case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
	{
		SDL_Gamepad* pad = SDL_GetGamepadFromID(ev.gbutton.which);
		auto button = SDL_GamepadButton(ev.gbutton.button);
		SDL_GamepadButtonLabel label = pad ? SDL_GetGamepadButtonLabel(pad, button) : SDL_GAMEPAD_BUTTON_LABEL_UNKNOWN;
		using namespace Rml::Input;
		// confirm and back by what the button says on it: A / cross, B / circle (A is on the right on a Nintendo pad)
		if (label == SDL_GAMEPAD_BUTTON_LABEL_A || label == SDL_GAMEPAD_BUTTON_LABEL_CROSS)
			key(KI_RETURN);
		else if (label == SDL_GAMEPAD_BUTTON_LABEL_B || label == SDL_GAMEPAD_BUTTON_LABEL_CIRCLE)
			back();
		else if (button == SDL_GAMEPAD_BUTTON_DPAD_UP)
			key(KI_UP);
		else if (button == SDL_GAMEPAD_BUTTON_DPAD_DOWN)
			key(KI_DOWN);
		else if (button == SDL_GAMEPAD_BUTTON_DPAD_LEFT)
			key(KI_LEFT);
		else if (button == SDL_GAMEPAD_BUTTON_DPAD_RIGHT)
			key(KI_RIGHT);
		return;
	}
	case SDL_EVENT_KEY_DOWN:
	{
		Rml::Input::KeyIdentifier k = RmlSDL::ConvertKey(int(ev.key.key));
		if (k == Rml::Input::KI_ESCAPE)
		{
			back();
			return;
		}
		if (key(k))
			return;
		break;
	}
	default: break;
	}
	RmlSDL::InputEventHandler(ctx, window, ev);
	if (ev.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED || ev.type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED)
		rescale();
}

// keys with a meaning of their own here; true when used. The rest go to RmlUi (arrows move the focus: `nav`).
bool App::key(Rml::Input::KeyIdentifier k)
{
	using namespace Rml::Input;
	Rml::Element* focus = ctx->GetFocusElement();
	bool editing = focus && focus->GetTagName() == "input";
	if ((k == KI_RETURN || k == KI_NUMPADENTER) && focus && !editing && focus != ctx->GetDocument(0))
	{
		focus->Click();
		return true;
	}
	if ((k == KI_LEFT || k == KI_RIGHT) && focus && focus->IsClassSet("setting"))
	{
		step_setting(std::atoi(focus->GetAttribute<Rml::String>("index", "0").c_str()), k == KI_LEFT ? -1 : 1);
		return true;
	}
	if (!focus || focus == ctx->GetDocument(0))
	{
		if (k == KI_UP || k == KI_DOWN || k == KI_RETURN)
		{
			focus_first();
			return true;
		}
	}
	ctx->ProcessKeyDown(k, RmlSDL::GetKeyModifierState());
	ctx->ProcessKeyUp(k, RmlSDL::GetKeyModifierState());
	return true;
}

void App::back()
{
	Rml::Element* focus = ctx->GetFocusElement();
	if (focus && focus->GetTagName() == "input")
	{
		focus->Blur();
		return;
	}
	if (screen != "title")
		go("title");
}

// ---------------------------------------------------------------------------------------------------- screens
void App::go(const Rml::String& to)
{
	screen = to;
	if (to == "title")
		refresh_title();
	else if (to == "mods")
		refresh_mods();
	else if (to == "settings")
		refresh_settings();
	else if (to == "build")
		refresh_build();
	model.DirtyVariable("screen");
	want_focus = true;
}

void App::refresh_title()
{
	built = file_exists(paths.build + "/" + GAME_EXE) && file_exists(paths.build + "/" + GAME_LIB) &&
		file_exists(paths.repo + "/targets/exefs/main.elf");
	status_bad = false;
	std::string tag, message;
	const update::State us = update::state(tag, message);
	update_tag = us == update::State::available || us == update::State::failed ? tag : "";
	if (us == update::State::working)
	{
		status_title = "UPDATING";
		status_line = message;
	}
	else if (us == update::State::failed)
	{
		status_bad = true;
		status_title = "UPDATE FAILED";
		status_line = message;
	}
	else if (updated && built)
	{
		status_title = "UPDATED";
		status_line = "Now " WT_VERSION ". Rebuild before you play";
	}
	else if (returned_code > 0)
	{
		status_bad = true;
		status_title = "THE GAME STOPPED";
		status_line = "Exit code " + std::to_string(returned_code) + ". Its log is in build/run.";
	}
	else if (built)
	{
		struct stat st;
		char when[64] = "";
		if (stat((paths.build + "/" + GAME_LIB).c_str(), &st) == 0)
			std::strftime(when, sizeof when, "%e %b, %H:%M", std::localtime(&st.st_mtime));
		std::string scale = ini_get(paths.config + "/graphics.ini", "", "resolution_scale", "1.0");
		status_title = returned_code == 0 ? "WELCOME BACK" : "READY";
		status_line = std::string("Built ") + (when[0] == ' ' ? when + 1 : when) + " · " + scale + "× internal";
	}
	else
	{
		status_title = "NOT BUILT YET";
		status_line = "Set up the build to play";
	}
	const char* driver = gpu ? SDL_GetGPUDeviceDriver(gpu) : nullptr;
	footer_info = std::string(WT_VERSION " · ") + (driver && !std::strcmp(driver, "vulkan") ? "Vulkan" : driver ? driver : "") +
		(pads.empty() ? " · Keyboard and mouse" : " · Controller connected");
	model.DirtyVariable("built");
	model.DirtyVariable("status_bad");
	model.DirtyVariable("status_title");
	model.DirtyVariable("status_line");
	model.DirtyVariable("footer_info");
	model.DirtyVariable("update_tag");
}

void App::refresh_build()
{
	const SetupState st = SetupState::read(paths.build);
	const auto& all = BuildRunner::steps();
	const bool run = job == Job::build && runner.running();
	const int cur = run ? step_index(runner.current) : -1;
	auto is_done = [&](const std::string& s) {
		const bool ran = std::find(runner.done.begin(), runner.done.end(), s) != runner.done.end();
		if (run)
			return cur >= 0 ? step_index(s) < cur : ran;
		return st.has(s) || ran;
	};
	progress = 0;
	for (const std::string& s : all)
		if (is_done(s))
			progress += weight(s);
	if (run && runner.current == "compile" && runner.to_compile > 0)
		progress += weight("compile") * runner.compiled / runner.to_compile;
	const bool complete = !run && std::all_of(all.begin(), all.end(), is_done);
	if (complete)
		progress = 1;
	progress = std::clamp(progress, 0.f, 1.f);
	percent = int(progress * 100 + .5f);

	steps.clear();
	for (const Group& g : groups())
	{
		StepRow r{g.num, g.name, g.note, "", "wait"};
		bool all_done = std::all_of(g.steps.begin(), g.steps.end(), is_done);
		bool here = run && std::find(g.steps.begin(), g.steps.end(), runner.current) != g.steps.end();
		bool failed = !run && std::find(g.steps.begin(), g.steps.end(), runner.failed) != g.steps.end();
		if (failed)
		{
			r.state = "fail";
			r.right = "Stopped";
		}
		else if (here)
		{
			r.state = "run";
			if (runner.current == "compile" && runner.to_compile > 0)
				r.note = std::to_string(runner.compiled) + " of " + std::to_string(runner.to_compile) + " files";
			r.right = phase_of(runner.current);
		}
		else if (all_done)
		{
			r.state = "done";
			std::string t;
			for (const std::string& s : g.steps)
				if (runner.took.count(s))
					t = runner.took.at(s);
			r.right = g.steps.size() == 1 && !t.empty() ? t : "Done";
		}
		else
			r.right = run ? "Next" : "";
		steps.push_back(r);
	}
	building = run;
	phase = run ? phase_of(runner.current) : complete ? "ready" : !runner.failed.empty() ? "stopped" : progress > 0 ? "paused" : "not started";
	build_aside = run ? "Step " + std::to_string(std::max(cur, 0) + 1) + " of 10 · " + minutes(now_seconds() - runner.started_at) + " so far" : "";
	log.clear();
	if (job == Job::build || !runner.lines.empty())
		for (size_t i = runner.lines.size() > 6 ? runner.lines.size() - 6 : 0; i < runner.lines.size(); i++)
			log.push_back(runner.lines[i]);
	for (const char* v : {"progress", "percent", "steps", "building", "phase", "build_aside", "log", "build_error"})
		model.DirtyVariable(v);
}

void App::refresh_mods()
{
	SetupState st = SetupState::read(paths.build);
	std::string error;
	mod_entries = list_mods(paths, st.mods_dir.empty() ? paths.mods : st.mods_dir, error);
	mods.clear();
	int on = 0, code_on = 0;
	for (const ModEntry& m : mod_entries)
	{
		mods.push_back({m.label, m.note.empty() ? (m.code ? (m.data ? "Code patches and data files" : "Code patches") : "Data files, read at start") : m.note,
			m.code ? (m.data ? "CODE + DATA" : "CODE") : "DATA", m.on});
		on += m.on;
		code_on += m.on && m.code;
	}
	mods_error = error;
	mods_summary = mods.empty() ? "No mods found." : std::to_string(on) + (on == 1 ? " mod on, " : " mods on, ") + std::to_string(code_on) +
		" of them in code. Changing a code mod rebuilds part of the game.";
	for (const char* v : {"mods", "mods_error", "mods_summary", "mods_rebuild"})
		model.DirtyVariable(v);
}

void App::toggle_mod(int index)
{
	if (index < 0 || index >= int(mod_entries.size()))
		return;
	ModEntry& m = mod_entries[size_t(index)];
	m.on = !m.on;
	ini_set(paths.config + "/mods.ini", "mods", m.key, m.on ? "on" : "off");
	if (m.code)
		mods_rebuild = true;
	refresh_mods();
}

void App::refresh_settings()
{
	const std::string ini = paths.config + "/graphics.ini";
	settings.clear();
	for (const SettingDef& d : setting_defs())
	{
		std::string v = ini_get(ini, "", d.key, d.options.front().first);
		int i = option_index(d, v);
		settings.push_back({d.name, d.note, i >= 0 ? d.options[size_t(i)].second : v});
	}
	model.DirtyVariable("settings");
}

void App::step_setting(int index, int dir)
{
	const auto& defs = setting_defs();
	if (index < 0 || index >= int(defs.size()))
		return;
	const SettingDef& d = defs[size_t(index)];
	const std::string ini = paths.config + "/graphics.ini";
	int i = option_index(d, ini_get(ini, "", d.key, d.options.front().first));
	int n = int(d.options.size());
	i = i < 0 ? 0 : (i + dir + n) % n;
	ini_set(ini, "", d.key, d.options[size_t(i)].first);
	refresh_settings();
}

// ---------------------------------------------------------------------------------------------------- work
void App::browse()
{
	SDL_ShowOpenFolderDialog(
		[](void* user, const char* const* list, int) {
			App* app = static_cast<App*>(user);
			if (list && list[0])
			{
				std::lock_guard<std::mutex> lock(app->picked_mutex);
				app->picked_folder = list[0];
				app->picked = true;
			}
		},
		this, window, dump.empty() ? nullptr : dump.c_str(), false);
}

void App::check_dump()
{
	if (runner.running() || dump.empty())
	{
		if (dump.empty())
		{
			checks = {{"Choose the folder of your dump first", "", "fail"}};
			model.DirtyVariable("checks");
		}
		return;
	}
	verified = false;
	checks = {{"Starting the checks", "", "run"}};
	std::vector<std::string> args = {"--dump", dump, "--only", "verify"};
	if (accept_modified)
		args.push_back("--accept-modified");
	if (runner.start(paths.repo, args))
	{
		job = Job::verify;
		checking = true;
	}
	else
		checks = {{"The build tool cannot be started", "", "fail"}};
	for (const char* v : {"checks", "checking", "verified"})
		model.DirtyVariable(v);
}

void App::start_build()
{
	if (runner.running())
		return;
	std::vector<std::string> args;
	if (!dump.empty())
		args = {"--dump", dump};
	if (move_dump)
		args.push_back("--move-dump");
	if (accept_modified)
		args.push_back("--accept-modified");
	build_error.clear();
	mods_rebuild = false;
	if (runner.start(paths.repo, args))
		job = Job::build;
	else
		build_error = "The build tool cannot be started.";
	go("build");
}

void App::tick_clock()
{
	std::time_t now = std::time(nullptr);
	std::tm* t = std::localtime(&now);
	if (!t)
		return;
	const float m = t->tm_min * 6 + t->tm_sec * 0.1f, h = (t->tm_hour % 12) * 30 + t->tm_min * 0.5f;
	if (m == minute_deg && h == hour_deg)
		return;
	minute_deg = m;
	hour_deg = h;
	model.DirtyVariable("minute_deg");
	model.DirtyVariable("hour_deg");
}

void App::tick()
{
	tick_clock();
	{
		std::lock_guard<std::mutex> lock(picked_mutex);
		if (picked)
		{
			picked = false;
			dump = picked_folder;
			verified = false;
			dump_modified = accept_modified = false;    // a new folder is checked as it is
			for (const char* v : {"dump", "verified", "dump_modified", "accept_modified"})
				model.DirtyVariable(v);
		}
	}
	{
		std::string tag, message;
		const update::State us = update::state(tag, message);
		if (us == update::State::done)
			running = false;               // run() then starts the new launcher
		else if (int(us) != update_seen || message != update_message)
		{
			update_seen = int(us);
			update_message = message;
			refresh_title();
		}
	}
	if (!runner.poll() && !(job == Job::build && runner.running()))
		return;
	if (job == Job::verify)
	{
		checks.clear();
		bool modified = false;
		for (const std::string& l : runner.lines)
		{
			std::string t = l.substr(std::min(l.size(), l.find_first_not_of(' ')));
			modified |= t.find("--accept-modified") != std::string::npos || t.rfind("warning:", 0) == 0;
			if (t.find("--accept-modified") != std::string::npos)    // the command-line advice; the switch above is the launcher's way
				t = "The dump differs from the supported version. Turn on \"Use a modified dump\" to build it anyway.";
			if (!t.empty() && t.rfind("setup:", 0) != 0 && t.rfind("Start the game:", 0) != 0)    // wtbuild's closing lines are not checks
				checks.push_back({t, "", "done"});
		}
		if (modified && !dump_modified)
		{
			dump_modified = true;
			model.DirtyVariable("dump_modified");
		}
		if (checks.size() > 8)
			checks.erase(checks.begin(), checks.end() - 8);
		if (runner.running())
			checks.push_back({phase_of(runner.current.empty() ? "deps" : runner.current), "", "run"});
		else
		{
			checking = false;
			verified = std::find(runner.done.begin(), runner.done.end(), "verify") != runner.done.end();
			if (!runner.failed.empty() && !checks.empty())
				checks.back().state = "fail";      // the line that says why
			checks.push_back(verified ? CheckRow{"Your dump is the version this port supports", "Matches", "done"}
									  : CheckRow{"The dump did not pass the checks above", "", "fail"});
			job = Job::none;
		}
		for (const char* v : {"checks", "checking", "verified"})
			model.DirtyVariable(v);
		return;
	}
	if (job == Job::build && !runner.running())
	{
		job = Job::none;
		if (runner.exit_code)
		{
			build_error.clear();
			bool after = false;
			for (const std::string& l : runner.lines)       // what the build printed after FAILED
			{
				if (after && build_error.size() < 600)
					build_error += (build_error.empty() ? "" : " ") + l.substr(std::min(l.size(), l.find_first_not_of(' ')));
				after |= l.find("FAILED") != std::string::npos;
			}
			if (build_error.empty())
				build_error = runner.failed.empty() ? "The build was stopped." : "The " + runner.failed + " step failed; see build/logs.";
		}
		else
			updated = false;                   // rebuilt with the update
		refresh_title();
	}
	refresh_build();
}

void App::release_window()
{
	if (!gpu)
		return;
	SDL_StopTextInput(window);
	ctx = nullptr;
	Rml::Shutdown();
	render->Shutdown();
	render.reset();
	system.reset();
	SDL_ReleaseWindowFromGPUDevice(gpu, window);
	SDL_DestroyGPUDevice(gpu);
	gpu = nullptr;
	for (SDL_Gamepad* p : pads)
		SDL_CloseGamepad(p);
	pads.clear();
}

void App::play()
{
	if (runner.running())
		return;
	GameStart g = prepare_game(paths);
	if (!g.error.empty())
	{
		play_only = false;                       // nothing to play: the launcher stays, and says why
		go("title");
		status_bad = true;
		status_title = "NOT READY";
		status_line = g.error;
		for (const char* v : {"status_bad", "status_title", "status_line"})
			model.DirtyVariable(v);
		return;
	}
	if (g.in_process)
	{
		release_window();
		run_in_process(g, paths, window, !play_only);    // does not come back
	}
	returned_code = run_child(paths, window);
	if (play_only)
		running = false;
	refresh_title();
}
