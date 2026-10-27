// Stand-in for SDL3_image, which RmlUi's SDL GPU renderer includes for its image files. The launcher draws its own
// pictures (ui_render.cpp); a file it is given is read as BMP, which SDL itself can load.
#pragma once
#include <SDL3/SDL.h>

static inline SDL_Surface* IMG_LoadTyped_IO(SDL_IOStream* src, bool closeio, const char* type)
{
	(void)type;
	return SDL_LoadBMP_IO(src, closeio);
}
