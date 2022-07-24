#include "painter/glyph_cache.h"

#include "painter/font.h"

static void lru_cache_use(exo::Pool<GlyphEntry> &cache, Handle<GlyphEntry> &head, Handle<GlyphEntry> handle)
{
	if (head == handle) {
		return;
	}

	// Remove the element from the list
	auto &element_prev = cache.get(handle).lru_prev;
	auto &element_next = cache.get(handle).lru_next;

	if (element_next.is_valid()) {
		cache.get(element_next).lru_prev = element_prev;
	}
	if (element_prev.is_valid()) {
		cache.get(element_prev).lru_next = element_next;
	}
	element_prev = Handle<GlyphEntry>::invalid();

	// Connect the head to the element
	if (head.is_valid()) {
		auto &head_prev = cache.get(head).lru_prev;
		ASSERT(head_prev.is_valid() == false);
		head_prev = handle;
	}

	// Connect the element to the head
	element_next = head;

	// The new head is the element
	head = handle;
}

static Handle<GlyphEntry> lru_cache_pop(exo::Pool<GlyphEntry> &cache, Handle<GlyphEntry> &head)
{
	if (head.is_valid() == false) {
		return {};
	}

	Handle<GlyphEntry> popped = head;

	auto head_next = cache.get(head).lru_next;

	// The new head is head_next
	if (head_next.is_valid()) {
		cache.get(head_next).lru_prev = Handle<GlyphEntry>::invalid();
	}
	head = head_next;

	return popped;
}

// Returns the pixel offset from the top left corner and atlas coords for a specified face and glyph
Option<int2> GlyphCache::queue_glyph(Font &font, GlyphId glyph_id, GlyphImage *image)
{
	auto &face_cache = this->face_caches[0];

	// Find an already allocated glyph
	for (const auto &glyph_key : face_cache) {
		if (glyph_key.glyph_id == glyph_id) {
			const auto &glyph_entry = this->lru_cache.get(glyph_key.handle);
			lru_cache_use(this->lru_cache, this->lru_head, glyph_key.handle);

			if (!glyph_entry.allocator_id.is_valid()) {
				return None;
			}

			if (image) {
				*image = glyph_entry.image;
			}

			const auto &atlas_alloc = this->allocator.get(glyph_entry.allocator_id);
			return Some(atlas_alloc.pos);
		}
	}

	// Not found, we need to rasterize it
	GlyphImage   glyph_image   = {};
	GlyphMetrics glyph_metrics = {};
	this->rasterizer(font, glyph_id, glyph_image, glyph_metrics);

	AllocationId alloc_id;
	if (glyph_image.image_size.x == 0 || glyph_image.image_size.y == 0) {
		alloc_id = AllocationId::invalid();
	} else {
		alloc_id = this->alloc_glyph(int2(glyph_image.image_size) + int2(2));
	}

	auto new_glyph_handle = this->lru_cache.add(GlyphEntry{
		.allocator_id = alloc_id,
		.glyph_id     = glyph_id,
		.image        = glyph_image,
		.metrics      = glyph_metrics,
	});
	lru_cache_use(this->lru_cache, this->lru_head, new_glyph_handle);
	face_cache.push_back(GlyphKey{
		.handle   = new_glyph_handle,
		.glyph_id = glyph_id,
	});

	this->events.push_back(GlyphEvent{
		.type         = GlyphEvent::Type::New,
		.glyph_handle = new_glyph_handle,
	});

	if (!alloc_id.is_valid()) {
		return None;
	}

	if (image) {
		*image = glyph_image;
	}

	const auto &atlas_alloc = this->allocator.get(alloc_id);
	return Some(atlas_alloc.pos);
}

AllocationId GlyphCache::alloc_glyph(int2 alloc_size)
{
	ASSERT(alloc_size.x > 0 && alloc_size.y > 0);
	auto alloc_id = this->allocator.alloc(alloc_size);
	while (!alloc_id.is_valid()) {
		// Evict the least recently used glyph
		auto evicted_glyph_handle   = lru_cache_pop(this->lru_cache, this->lru_head);
		auto evicted_glyph_alloc_id = this->lru_cache.get(evicted_glyph_handle).allocator_id;
		this->lru_cache.remove(evicted_glyph_handle);

		// Remove it from the face cache
		auto &face_cache  = this->face_caches[0];
		usize i_glyph_key = 0;
		for (; i_glyph_key < face_cache.size(); ++i_glyph_key) {
			if (face_cache[i_glyph_key].handle == evicted_glyph_handle) {
				break;
			}
		}
		exo::swap_remove(face_cache, i_glyph_key);

		// Deallocate it in the atlas
		auto glyph_removed = this->allocator.unref(evicted_glyph_alloc_id);

		if (glyph_removed) {
			// Try to allocate it again
			alloc_id = this->allocator.alloc(alloc_size);
		}
	}
	return alloc_id;
}

void GlyphCache::clear_events() { this->events.clear(); }
