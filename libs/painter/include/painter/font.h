#pragma once
#include <exo/maths/numerics.h>
struct hb_font_t;
struct GlyphImage;
struct GlyphMetrics;

struct FontMetrics
{
	i32 height    = 0;
	i32 ascender  = 0;
	i32 descender = 0;
};

struct Font
{
	hb_font_t  *hb_font = nullptr;
	FontMetrics metrics = {};

	static Font from_file(const char *path, i32 size_in_pt, i32 face_index = 0);
};

void freetype_rasterizer(Font &font, u32 glyph_id, GlyphImage &out_image, GlyphMetrics &out_metrics);
