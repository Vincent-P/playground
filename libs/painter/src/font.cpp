#include "painter/font.h"

#include "exo/macros/assert.h"
#include "exo/maths/vectors.h"
#include "exo/profile.h"
#include "painter/glyph_cache.h"
#include <cstring> // for memcpy

#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb-ft.h>
#include <hb.h>

FT_Library *global_library = nullptr;

Font Font::from_file(const char *path, i32 size_in_pt, i32 face_index)
{
	if (!global_library) {
		auto *library = reinterpret_cast<FT_Library *>(malloc(sizeof(FT_Library)));
		auto  error   = FT_Init_FreeType(library);
		ASSERT(!error);
		global_library = library;

		EXO_PROFILE_MALLOC(global_library, sizeof(FT_Library));
	}

	Font res = {};

	FT_Face new_face = nullptr;
	auto    error    = FT_New_Face(*global_library, path, face_index, &new_face);
	ASSERT(!error);

	FT_Set_Char_Size(new_face, 0, size_in_pt * 64, 0, 96);

	res.hb_font = hb_ft_font_create_referenced(new_face);
	hb_ft_font_set_funcs(res.hb_font);

	FT_Done_Face(new_face);

	res.metrics.height    = new_face->size->metrics.height >> 6;
	res.metrics.ascender  = new_face->size->metrics.ascender >> 6;
	res.metrics.descender = new_face->size->metrics.descender >> 6;

	return res;
}

void freetype_rasterizer(Font &font, u32 glyph_id, GlyphImage &out_image, GlyphMetrics & /*out_metrics*/)
{
	FT_Face face = hb_ft_font_get_face(font.hb_font);

	FT_GlyphSlot slot = face->glyph;

	int error = 0;
	error     = FT_Load_Glyph(face, glyph_id, 0);
	ASSERT(!error);

	error = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
	ASSERT(!error);

	// Upload it to GPU
	const uint2 bitmap_size = uint2(slot->bitmap.width, slot->bitmap.rows);
	void       *data        = slot->bitmap.buffer;
	const i32   data_size   = slot->bitmap.pitch * static_cast<i32>(slot->bitmap.rows);

	ASSERT(data_size >= 0);

	out_image.data_size = static_cast<u32>(data_size);

	out_image.data = std::malloc(out_image.data_size);
	EXO_PROFILE_MALLOC(out_image.data, sizeof(out_image.data_size));
	std::memcpy(out_image.data, data, out_image.data_size);

	out_image.image_size = bitmap_size;
	out_image.top_left.x = slot->bitmap_left;
	out_image.top_left.y = slot->bitmap_top;
}
