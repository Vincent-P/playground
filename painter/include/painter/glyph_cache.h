#pragma once
#include <exo/collections/pool.h>
#include <exo/collections/vector.h>
#include <exo/maths/numerics.h>
#include <exo/maths/vectors.h>
#include <exo/option.h>

#include "shelf_allocator.h"

struct Font;
using GlyphId = u32;
struct GlyphEntry;

// A rasterized glyph
struct GlyphImage
{
	void *data       = nullptr;
	usize data_size  = 0;
	int2  top_left   = int2(0);
	uint2 image_size = uint2(0);
};

struct GlyphMetrics
{};

// Event given to the application to upload glyph correctly

struct GlyphEvent
{
	enum Type
	{
		Invalid,
		New,
		Evicted,
	};

	Type               type         = Type::Invalid;
	Handle<GlyphEntry> glyph_handle = {};
};

// A glyph that is in the cache
struct GlyphEntry
{
	AllocationId allocator_id;
	GlyphId      glyph_id;
	GlyphImage   image;
	GlyphMetrics metrics;

	Handle<GlyphEntry> lru_prev;
	Handle<GlyphEntry> lru_next;
};

// This struct is used instead of Handle<GlyphEntry> to make it easier to find a given glyph_id
struct GlyphKey
{
	Handle<GlyphEntry> handle;
	GlyphId            glyph_id;
};

// The rasterizer can be changed anytime
using RasterizerFn = void (*)(Font &font, u32 glyph_id, GlyphImage &out_image, GlyphMetrics &out_metrics);

struct GlyphCache
{
	ShelfAllocator        allocator  = {};
	Vec<GlyphEvent>       events     = {};
	exo::Pool<GlyphEntry> lru_cache  = {};
	Handle<GlyphEntry>    lru_head   = {};
	RasterizerFn          rasterizer = nullptr;

	// TODO: Per-face
	Vec<GlyphKey> face_caches[1] = {};

	// Returns the pixel offset from the top left corner and atlas coords for a specified face and glyph
	Option<int2> queue_glyph(Font &font, GlyphId glyph_id, GlyphImage *image);

	template <typename Lambda> void process_events(Lambda fn) const
	{
		for (const auto &event : this->events) {
			const GlyphImage *image    = nullptr;
			int2              position = int2(0);
			if (event.type == GlyphEvent::Type::New) {
				const auto &entry = this->lru_cache.get(event.glyph_handle);
				if (entry.allocator_id.is_valid()) {
					image    = &entry.image;
					position = this->allocator.get(entry.allocator_id).pos;
				}
			}
			fn(event, image, position);
		}
	}
	void clear_events();

private:
	AllocationId alloc_glyph(int2 alloc_size);
};
