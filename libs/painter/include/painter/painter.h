#pragma once
#include "exo/collections/map.h"
#include "exo/collections/span.h"
#include "exo/macros/packed.h"
#include "exo/maths/vectors.h"
#include "exo/string_view.h"
#include "painter/color.h"
#include "painter/glyph_cache.h"
#include "painter/rect.h"

namespace exo
{
struct ScopeStack;
}
struct Font;
struct hb_font_t;
struct hb_buffer_t;
struct hb_glyph_info_t;
struct hb_glyph_position_t;

PACKED(struct ColorRect {
	Rect rect;
	u32 color;
	u32 i_clip_rect;
	u32 padding[2];
})

PACKED(struct SdfRect {
	Rect rect;
	u32 color;
	u32 i_clip_rect;
	u32 border_color;
	u32 border_thickness;
})
static_assert(sizeof(SdfRect) == sizeof(ColorRect));

PACKED(struct TexturedRect {
	Rect rect;
	Rect uv;
	u32 texture_descriptor;
	u32 i_clip_rect;
	u32 padding[2];
})

enum RectType : u32
{
	RectType_Color = 0,
	RectType_Textured = 1,
	RectType_Clip = 2,
	RectType_Sdf_RoundRectangle = (0b100000),
	RectType_Sdf_Circle = (0b100001)
};

union PrimitiveIndex
{
	struct
	{
		u32 index : 24;
		u32 corner : 2;
		u32 type : 6;
	} bits;
	u32 raw;
};
static_assert(sizeof(PrimitiveIndex) == sizeof(u32));

struct CachedRun
{
	hb_buffer_t *hb_buf = nullptr;
	hb_glyph_info_t *glyph_infos = nullptr;
	hb_glyph_position_t *glyph_positions = nullptr;
	u32 glyph_count = 0;
};

struct ShapeContext
{
	exo::Map<exo::StringView, CachedRun> cached_runs;

	// --
	static ShapeContext create();
	const CachedRun &get_run(Font &font, exo::StringView text_run);
};

struct Painter
{
	GlyphCache glyph_cache = {};
	ShapeContext shaper = {};
	exo::Span<u8> vertex_buffer = {};
	exo::Span<PrimitiveIndex> index_buffer = {};
	usize vertex_bytes_offset = 0;
	u32 index_offset = 0;
	u32 glyph_atlas_gpu_idx = u32_invalid;

	// --

	static Painter create(exo::Span<u8> vbuffer, exo::Span<PrimitiveIndex> ibuffer, int2 glyph_cache_size);

	void draw_textured_rect(const Rect &r, u32 i_clip_rect, const Rect &uv, u32 texture_id);
	void draw_color_rect(const Rect &r, u32 i_clip_rect, ColorU32 c);

	int2 measure_label(Font &r, exo::StringView label);
	void draw_label(const Rect &r, u32 i_clip_rect, Font &font, exo::StringView label);

	void draw_color_round_rect(const Rect &r, u32 i_clip_rect, ColorU32 c, ColorU32 border_c, u32 border_thickness);
	void draw_color_circle(const Rect &r, u32 i_clip_rect, ColorU32 c, ColorU32 border_c, u32 border_thickness);
};
