#include "ui_render.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>
#include <vector>

// Pictures by name, with parameters after '?' (colors as rrggbb, alpha 0-255):
//   gen:glow?x=&y=&rx=&ry=&c1=&c0=&stop=   soft elliptic glow c1 at (x, y) fading to c0 at stop (the screen backgrounds)
//   gen:ring?c=&a=&w=                      a circle line w texels thick in a 512 texel square (dial rings, progress)
//   gen:ticks?n=&c=&a=&w=&l=               n marks w texels wide and l long around the edge (the dial's minutes)
//   gen:moon?c=                            a crescent (the dial's centre)
//   gen:fade?c=&a=                         c fading out from left to right (the selected menu row's line)
namespace {

struct Params {
	std::map<Rml::String, Rml::String> v;
	float num(const char* k, float def) const
	{
		auto it = v.find(k);
		return it == v.end() ? def : std::strtof(it->second.c_str(), nullptr);
	}
	Rml::Colourb color(const char* k, Rml::Colourb def) const
	{
		auto it = v.find(k);
		if (it == v.end() || it->second.size() != 6)
			return def;
		unsigned long x = std::strtoul(it->second.c_str(), nullptr, 16);
		return Rml::Colourb(Rml::byte(x >> 16), Rml::byte(x >> 8), Rml::byte(x), 255);
	}
};

Params parse(const Rml::String& query)
{
	Params p;
	size_t i = 0;
	while (i < query.size())
	{
		size_t amp = query.find('&', i);
		if (amp == Rml::String::npos)
			amp = query.size();
		Rml::String kv = query.substr(i, amp - i);
		size_t eq = kv.find('=');
		if (eq != Rml::String::npos)
			p.v[kv.substr(0, eq)] = kv.substr(eq + 1);
		i = amp + 1;
	}
	return p;
}

struct Image {
	int w, h;
	std::vector<Rml::byte> px;
	Image(int w, int h) : w(w), h(h), px(size_t(w) * h * 4, 0) {}
	// premultiplied, as the renderer wants it
	void put(int x, int y, Rml::Colourb c, float a)
	{
		a = std::clamp(a, 0.f, 1.f) * (c.alpha / 255.f);
		Rml::byte* p = &px[(size_t(y) * w + x) * 4];
		p[0] = Rml::byte(c.red * a + .5f);
		p[1] = Rml::byte(c.green * a + .5f);
		p[2] = Rml::byte(c.blue * a + .5f);
		p[3] = Rml::byte(255 * a + .5f);
	}
};

float coverage(float d) { return std::clamp(.5f - d, 0.f, 1.f); }   // signed distance in texels -> antialiased coverage

Image glow(const Params& p)
{
	Image im(320, 200);
	const float cx = p.num("x", .5f), cy = p.num("y", .5f), rx = p.num("rx", .7f), ry = p.num("ry", .9f), stop = p.num("stop", .62f);
	const Rml::Colourb c1 = p.color("c1", {36, 17, 46, 255}), c0 = p.color("c0", {10, 6, 16, 255});
	for (int y = 0; y < im.h; y++)
		for (int x = 0; x < im.w; x++)
		{
			float dx = ((x + .5f) / im.w - cx) / rx, dy = ((y + .5f) / im.h - cy) / ry;
			float t = std::clamp(std::sqrt(dx * dx + dy * dy) / stop, 0.f, 1.f);
			t = t * t * (3 - 2 * t);
			Rml::Colourb c(Rml::byte(c1.red + (c0.red - c1.red) * t), Rml::byte(c1.green + (c0.green - c1.green) * t),
				Rml::byte(c1.blue + (c0.blue - c1.blue) * t), 255);
			im.put(x, y, c, 1);
		}
	return im;
}

Image ring(const Params& p)
{
	Image im(512, 512);
	Rml::Colourb c = p.color("c", {201, 163, 90, 255});
	c.alpha = Rml::byte(p.num("a", 255));
	const float w = p.num("w", 2), r = 256 - 1 - w / 2;
	for (int y = 0; y < im.h; y++)
		for (int x = 0; x < im.w; x++)
		{
			float d = std::fabs(std::hypot(x + .5f - 256, y + .5f - 256) - r) - w / 2;
			if (d < 1)
				im.put(x, y, c, coverage(d));
		}
	return im;
}

Image ticks(const Params& p)
{
	Image im(512, 512);
	Rml::Colourb c = p.color("c", {201, 163, 90, 255});
	c.alpha = Rml::byte(p.num("a", 255));
	const int n = std::max(1, int(p.num("n", 60)));
	const float w = p.num("w", 3), len = p.num("l", 14), r1 = 255 - len, r2 = 255;
	for (int y = 0; y < im.h; y++)
		for (int x = 0; x < im.w; x++)
		{
			float dx = x + .5f - 256, dy = y + .5f - 256, r = std::hypot(dx, dy);
			if (r < r1 - 1 || r > r2 + 1)
				continue;
			float step = 2 * float(M_PI) / n, ang = std::atan2(dy, dx);
			float k = std::round(ang / step) * step;                      // nearest mark
			float across = std::fabs(-dx * std::sin(k) + dy * std::cos(k)) - w / 2;
			float along = std::max(r1 - r, r - r2);
			im.put(x, y, c, coverage(std::max(across, along)));
		}
	return im;
}

Image moon(const Params& p)
{
	Image im(256, 256);
	const Rml::Colourb c = p.color("c", {232, 207, 148, 255});
	for (int y = 0; y < im.h; y++)
		for (int x = 0; x < im.w; x++)
		{
			float outer = std::hypot(x + .5f - 128, y + .5f - 128) - 120;
			float inner = std::hypot(x + .5f - 170, y + .5f - 104) - 104;  // the bite out of it
			im.put(x, y, c, coverage(std::max(outer, -inner)));
		}
	return im;
}

Image fade(const Params& p)
{
	Image im(256, 2);
	Rml::Colourb c = p.color("c", {224, 51, 138, 255});
	c.alpha = Rml::byte(p.num("a", 255));
	for (int y = 0; y < im.h; y++)
		for (int x = 0; x < im.w; x++)
			im.put(x, y, c, 1 - (x + .5f) / im.w);
	return im;
}

} // namespace

Rml::TextureHandle UiRender::LoadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source)
{
	const size_t at = source.find("gen:");
	if (at == Rml::String::npos)
		return RenderInterface_SDL_GPU::LoadTexture(texture_dimensions, source);
	Rml::String spec = source.substr(at + 4), name = spec, query;
	if (size_t q = spec.find('?'); q != Rml::String::npos)
	{
		name = spec.substr(0, q);
		query = spec.substr(q + 1);
	}
	const Params p = parse(query);
	Image im = name == "glow" ? glow(p) : name == "ring" ? ring(p) : name == "ticks" ? ticks(p) : name == "moon" ? moon(p) : fade(p);
	texture_dimensions = {im.w, im.h};
	return GenerateTexture({im.px.data(), im.px.size()}, texture_dimensions);
}
