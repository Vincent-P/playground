#pragma once
#include <exo/maths/numerics.h>
#include <exo/collections/handle.h>
namespace vulkan { struct Image; }
namespace gfx = vulkan;

struct FT_FaceRec_;
typedef FT_FaceRec_ *FT_Face;
struct hb_font_t;
struct hb_buffer_t;
struct GlyphCache;

struct Font
{
    i32          size_pt;
    u32 glyph_width_px;
    u32 glyph_height_px;
    u32          cache_resolution;

    FT_Face     ft_face;
    hb_font_t   *hb_font   = nullptr;
    hb_buffer_t *label_buf = nullptr;

    GlyphCache  *glyph_cache;
    exo::Handle<gfx::Image> glyph_atlas;
    u32 glyph_atlas_gpu_idx;
};
