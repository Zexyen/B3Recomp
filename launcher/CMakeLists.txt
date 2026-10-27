# Witch Time: the launcher. Builds the game from the player's dump (scripts/setup.py) and plays it in the same window
# (build/libwitchtime.so). SDL3 + RmlUi on SDL's GPU API; Linux first.
#
#     cmake -S launcher -B build/launcher -G Ninja -DCMAKE_BUILD_TYPE=Release
#     cmake --build build/launcher                      -> build/witch-time
#
# Needs the system's SDL3 (3.2 or newer) and FreeType. RmlUi is fetched at the pinned tag below, or taken from
# -DRMLUI_SOURCE_DIR=<folder> (an RmlUi checkout of that tag) when there is no network.
cmake_minimum_required(VERSION 3.20)
project(witch_time LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif()

find_package(SDL3 REQUIRED CONFIG)
find_package(Freetype REQUIRED)

set(RMLUI_TAG 6.3)
set(RMLUI_SOURCE_DIR "" CACHE PATH "An RmlUi ${RMLUI_TAG} checkout to use instead of fetching it")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(RMLUI_SAMPLES OFF CACHE BOOL "" FORCE)
set(RMLUI_FONT_ENGINE freetype CACHE STRING "" FORCE)
set(RMLUI_PRECOMPILED_HEADERS OFF CACHE BOOL "" FORCE)
include(FetchContent)
if(RMLUI_SOURCE_DIR)
	FetchContent_Declare(rmlui SOURCE_DIR "${RMLUI_SOURCE_DIR}")
else()
	FetchContent_Declare(rmlui GIT_REPOSITORY https://github.com/mikke89/RmlUi.git GIT_TAG ${RMLUI_TAG} GIT_SHALLOW TRUE)
endif()
FetchContent_MakeAvailable(rmlui)
set(RMLUI_BACKENDS "${rmlui_SOURCE_DIR}/Backends")

add_executable(witch-time
	src/main.cpp
	src/app.cpp
	src/build_runner.cpp
	src/config.cpp
	src/game_host.cpp
	src/ui_render.cpp
	"${RMLUI_BACKENDS}/RmlUi_Platform_SDL.cpp"
	"${RMLUI_BACKENDS}/RmlUi_Renderer_SDL_GPU.cpp")
# src/shim: <SDL3_image/SDL_image.h> for RmlUi's SDL GPU renderer, without the SDL3_image library (ui_render.cpp
# makes every picture the screens use; files are loaded as BMP)
target_include_directories(witch-time PRIVATE src src/shim "${RMLUI_BACKENDS}")
set(WT_VERSION "dev build" CACHE STRING "The version shown in the launcher (release builds pass the tag, e.g. v0.1.0)")
target_compile_definitions(witch-time PRIVATE RMLUI_SDL_VERSION_MAJOR=3 WT_VERSION="${WT_VERSION}")
target_link_libraries(witch-time PRIVATE RmlUi::Core SDL3::SDL3 ${CMAKE_DL_LIBS})
# beside witch-time-game, game.so and libwitchtime.so: the launcher finds the repository from its own place (build/..)
# $ORIGIN: a release ships libSDL3.so.0 beside it, which libwitchtime.so (also $ORIGIN) then shares
set_target_properties(witch-time PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../build"
	BUILD_RPATH "$ORIGIN")
