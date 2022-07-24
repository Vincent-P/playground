#pragma once
#include <exo/macros/packed.h>
#include <exo/maths/vectors.h>

#include "glyph_cache.h"
#include "rect.h"

namespace exo
{
struct ScopeStack;
}
struct Font;

PACKED(struct ColorRect {
	Rect rect;
	u32  color;
	u32  i_clip_rect;
	u32  padding[2];
})

PACKED(struct SdfRect {
	Rect rect;
	u32  color;
	u32  i_clip_rect;
	u32  border_color;
	u32  border_thickness;
})
static_assert(sizeof(SdfRect) == sizeof(ColorRect));

PACKED(struct TexturedRect {
	Rect rect;
	Rect uv;
	u32  texture_descriptor;
	u32  i_clip_rect;
	u32  padding[2];
})

enum RectType : u32
{
	RectType_Color              = 0,
	RectType_Textured           = 1,
	RectType_Clip               = 2,
	RectType_Sdf_RoundRectangle = (0b100000 + 0),
	RectType_Sdf_Circle         = (0b100000 + 1)
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

struct hb_font_t;
struct hb_buffer_t;
struct ShapeContext
{
	hb_buffer_t *hb_buf = nullptr;
};

struct Painter
{
	u8   *vertices      = nullptr;
	usize vertices_size = 0;

	PrimitiveIndex *indices      = nullptr;
	usize           indices_size = 0;

	usize vertex_bytes_offset = 0;
	u32   index_offset        = 0;

	GlyphCache   glyph_cache         = {};
	u32          glyph_atlas_gpu_idx = u32_invalid;
	ShapeContext shaper              = {};
};

Painter *painter_allocate(
	exo::ScopeStack &scope, usize vertex_buffer_size, usize index_buffer_size, int2 glyph_cache_size);
void painter_draw_textured_rect(Painter &painter, const Rect &rect, u32 i_clip_rect, const Rect &uv, u32 texture);
void painter_draw_color_rect(Painter &painter, const Rect &rect, u32 i_clip_rect, u32 AABBGGRR);
int2 measure_label(Painter &painter, Font &font, const char *label);
void painter_draw_label(Painter &painter, const Rect &rect, u32 i_clip_rect, Font &font, const char *label);
void painter_draw_color_round_rect(
	Painter &painter, const Rect &rect, u32 i_clip_rect, u32 color, u32 border_color, u32 border_thickness);
void painter_draw_color_circle(
	Painter &painter, const Rect &rect, u32 i_clip_rect, u32 color, u32 border_color, u32 border_thickness);
