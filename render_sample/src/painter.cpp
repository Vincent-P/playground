#include "painter.h"

#include "font.h"
#include "glyph_cache.h"

#include <cstring> // for std::memset
#include <exo/collections/vector.h>
#include <exo/macros/assert.h>
#include <exo/memory/scope_stack.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb-ft.h>
#include <hb.h>

Painter *painter_allocate(exo::ScopeStack &scope, usize vertex_buffer_size, usize index_buffer_size)
{
	auto *painter     = scope.allocate<Painter>();
	painter->vertices = reinterpret_cast<u8 *>(scope.allocate(vertex_buffer_size));
	painter->indices  = reinterpret_cast<PrimitiveIndex *>(scope.allocate(index_buffer_size));

	painter->vertices_size = vertex_buffer_size;
	painter->indices_size  = index_buffer_size;

	std::memset(painter->vertices, 0, vertex_buffer_size);
	std::memset(painter->indices, 0, index_buffer_size);

	painter->vertex_bytes_offset = 0;
	painter->index_offset        = 0;
	return painter;
}

void painter_draw_textured_rect(Painter &painter, const Rect &rect, u32 i_clip_rect, const Rect &uv, u32 texture)
{
	auto misalignment = painter.vertex_bytes_offset % sizeof(TexturedRect);
	if (misalignment != 0) {
		painter.vertex_bytes_offset += sizeof(TexturedRect) - misalignment;
	}

	ASSERT(painter.vertex_bytes_offset % sizeof(TexturedRect) == 0);
	u32   i_rect     = static_cast<u32>(painter.vertex_bytes_offset / sizeof(TexturedRect));
	auto *vertices   = reinterpret_cast<TexturedRect *>(painter.vertices);
	vertices[i_rect] = {.rect = rect, .uv = uv, .texture_descriptor = texture, .i_clip_rect = i_clip_rect};
	painter.vertex_bytes_offset += sizeof(TexturedRect);

	// 0 - 3
	// |   |
	// 1 - 2
	painter.indices[painter.index_offset++] = {{.index = i_rect, .corner = 0, .type = RectType_Textured}};
	painter.indices[painter.index_offset++] = {{.index = i_rect, .corner = 1, .type = RectType_Textured}};
	painter.indices[painter.index_offset++] = {{.index = i_rect, .corner = 2, .type = RectType_Textured}};
	painter.indices[painter.index_offset++] = {{.index = i_rect, .corner = 2, .type = RectType_Textured}};
	painter.indices[painter.index_offset++] = {{.index = i_rect, .corner = 3, .type = RectType_Textured}};
	painter.indices[painter.index_offset++] = {{.index = i_rect, .corner = 0, .type = RectType_Textured}};

	ASSERT(painter.index_offset * sizeof(PrimitiveIndex) < painter.indices_size);
	ASSERT(painter.vertex_bytes_offset < painter.vertices_size);

	exo::vector_insert_unique(painter.used_textures, texture);
}

void painter_draw_color_rect(Painter &painter, const Rect &rect, u32 i_clip_rect, u32 AABBGGRR)
{
	// Don't draw invisible rects
	if ((AABBGGRR & 0xFF000000) == 0) {
		return;
	}

	auto misalignment = painter.vertex_bytes_offset % sizeof(ColorRect);
	if (misalignment != 0) {
		painter.vertex_bytes_offset += sizeof(ColorRect) - misalignment;
	}

	ASSERT(painter.vertex_bytes_offset % sizeof(ColorRect) == 0);
	u32   i_rect     = static_cast<u32>(painter.vertex_bytes_offset / sizeof(ColorRect));
	auto *vertices   = reinterpret_cast<ColorRect *>(painter.vertices);
	vertices[i_rect] = {.rect = rect, .color = AABBGGRR, .i_clip_rect = i_clip_rect};
	painter.vertex_bytes_offset += sizeof(ColorRect);

	// 0 - 3
	// |   |
	// 1 - 2
	painter.indices[painter.index_offset++] = {{.index = i_rect, .corner = 0, .type = RectType_Color}};
	painter.indices[painter.index_offset++] = {{.index = i_rect, .corner = 1, .type = RectType_Color}};
	painter.indices[painter.index_offset++] = {{.index = i_rect, .corner = 2, .type = RectType_Color}};
	painter.indices[painter.index_offset++] = {{.index = i_rect, .corner = 2, .type = RectType_Color}};
	painter.indices[painter.index_offset++] = {{.index = i_rect, .corner = 3, .type = RectType_Color}};
	painter.indices[painter.index_offset++] = {{.index = i_rect, .corner = 0, .type = RectType_Color}};

	ASSERT(painter.index_offset * sizeof(PrimitiveIndex) < painter.indices_size);
	ASSERT(painter.vertex_bytes_offset < painter.vertices_size);
}

exo::int2 measure_label(Font *font, const char *label)
{
	auto *buf = font->label_buf;
	hb_buffer_clear_contents(buf);
	hb_buffer_add_utf8(buf, label, -1, 0, -1);
	hb_buffer_set_direction(buf, HB_DIRECTION_LTR);
	hb_buffer_set_script(buf, HB_SCRIPT_LATIN);
	hb_buffer_set_language(buf, hb_language_from_string("en", -1));

	hb_shape(font->hb_font, buf, nullptr, 0);

	u32 glyph_count;
	i32 line_height = (font->ft_face->size->metrics.ascender - font->ft_face->size->metrics.descender) >> 6;
	// hb_glyph_info_t     *glyph_info  = hb_buffer_get_glyph_infos(buf, &glyph_count);
	hb_glyph_position_t *glyph_pos = hb_buffer_get_glyph_positions(buf, &glyph_count);

	i32 cursor_x = 0;
	for (u32 i = 0; i < glyph_count; i++) {
		// auto &cache_entry = font->glyph_cache->get_or_create(glyph_info[i].codepoint);
		cursor_x += (glyph_pos[i].x_advance >> 6);
	}

	return {cursor_x, line_height};
}

void painter_draw_label(Painter &painter, const Rect &view_rect, u32 i_clip_rect, Font *font, const char *label)
{
	auto *buf = font->label_buf;
	hb_buffer_clear_contents(buf);
	hb_buffer_add_utf8(buf, label, -1, 0, -1);
	hb_buffer_set_direction(buf, HB_DIRECTION_LTR);
	hb_buffer_set_script(buf, HB_SCRIPT_LATIN);
	hb_buffer_set_language(buf, hb_language_from_string("en", -1));

	hb_shape(font->hb_font, buf, nullptr, 0);

	u32                  glyph_count;
	i32                  line_height = font->ft_face->size->metrics.height >> 6;
	hb_glyph_info_t     *glyph_info  = hb_buffer_get_glyph_infos(buf, &glyph_count);
	hb_glyph_position_t *glyph_pos   = hb_buffer_get_glyph_positions(buf, &glyph_count);

	i32 cursor_x = i32(view_rect.position.x);
	i32 cursor_y = i32(view_rect.position.y) + (font->ft_face->size->metrics.ascender >> 6);
	for (u32 i = 0; i < glyph_count; i++) {
		u32 glyph_index = glyph_info[i].codepoint;
		i32 x_advance   = glyph_pos[i].x_advance;
		i32 y_advance   = glyph_pos[i].y_advance;

		auto &cache_entry = font->glyph_cache->get_or_create(glyph_index);
		if (cache_entry.uploaded == false) {
			exo::vector_insert_unique(painter.glyphs_to_upload, {.font = font, .glyph_index = glyph_index});
		}

		Rect rect = {
			.position = exo::float2(exo::int2{cursor_x + cache_entry.glyph_top_left.x, cursor_y - cache_entry.glyph_top_left.y}),
			.size     = exo::float2(cache_entry.glyph_size),
		};
		Rect uv = {.position = {(cache_entry.x * font->glyph_width_px) / float(font->cache_resolution), (cache_entry.y * font->glyph_height_px) / float(font->cache_resolution)},
			   .size     = {cache_entry.glyph_size.x / float(font->cache_resolution), cache_entry.glyph_size.y / float(font->cache_resolution)}};
		painter_draw_textured_rect(painter, rect, i_clip_rect, uv, font->glyph_atlas_gpu_idx);

		cursor_x += x_advance >> 6;
		cursor_y += y_advance >> 6;

		if (label[glyph_info[i].cluster] == '\n') {
			cursor_x = i32(view_rect.position.x);
			cursor_y += line_height;
		}
	}
}
