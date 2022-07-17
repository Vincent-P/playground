#include "shelf_allocator.h"

#include <exo/collections/array.h>
#include <exo/macros/assert.h>

static i32 shelf_alloc(ShelfAllocator &allocator, Shelf &shelf, int2 alloc_size, i32 id)
{
	if (alloc_size.x > shelf.free) {
		return -1;
	}

	Allocation alloc = {
		.pos      = {shelf.size.x - shelf.free, shelf.y},
		.size     = alloc_size,
		.id       = id,
		.refcount = 1,
	};

	shelf.free -= alloc_size.x;
	allocator.allocations[id] = alloc;

	return id;
}

static i32 freelist_alloc(ShelfAllocator &allocator, u32 i_freelist, int2 alloc_size, i32 id)
{
	auto &new_alloc = allocator.freelist[i_freelist];
	ASSERT(new_alloc.capacity.x >= alloc_size.x);
	ASSERT(new_alloc.capacity.y >= alloc_size.y);
	new_alloc.alloc.id       = id;
	new_alloc.alloc.refcount = 1;
	new_alloc.alloc.size     = alloc_size;
	return id;
}

i32 ShelfAllocator::alloc(int2 alloc_size)
{
	i32 id = this->gen;
	this->gen += 1;

	i32 y               = 0;
	u32 i_best_shelf    = u32_invalid;
	u32 i_best_freelist = u32_invalid;
	i32 area_waste      = 99999;

	// find freelist
	for (u32 i_freelist = 0; i_freelist < this->freelist.size(); ++i_freelist) {
		const auto &freealloc = this->freelist[i_freelist];
		if (alloc_size == freealloc.capacity) {
			return freelist_alloc(*this, i_freelist, alloc_size, id);
		}

		if (alloc_size.y > freealloc.capacity.y || alloc_size.x > freealloc.capacity.x) {
			continue;
		}

		ASSERT(alloc_size.y < freealloc.capacity.y);
		ASSERT(alloc_size.x < freealloc.capacity.x);
		i32 waste = (freealloc.capacity.x * freealloc.capacity.y) - (alloc_size.x * alloc_size.y);
		if (waste < area_waste) {
			area_waste      = waste;
			i_best_freelist = i_freelist;
		}
	}

	// find shelf
	for (u32 i_shelf = 0; i_shelf < this->shelves.size(); ++i_shelf) {
		auto &shelf = this->shelves[i_shelf];
		y += shelf.size.y;

		if (alloc_size.x > shelf.free) {
			continue;
		}

		if (alloc_size.y == shelf.size.y) {
			return shelf_alloc(*this, shelf, alloc_size, id);
		}

		if (alloc_size.y > shelf.size.y) {
			continue;
		}

		ASSERT(alloc_size.y < shelf.size.y);
		i32 waste = (shelf.size.y - alloc_size.y) * alloc_size.x;
		if (waste < area_waste) {
			area_waste      = waste;
			i_best_shelf    = i_shelf;
			i_best_freelist = u32_invalid;
		}
	}

	if (i_best_freelist != u32_invalid) {
		return freelist_alloc(*this, i_best_freelist, alloc_size, id);
	}

	if (i_best_shelf != u32_invalid) {
		return shelf_alloc(*this, this->shelves[i_best_shelf], alloc_size, id);
	}

	if (alloc_size.y < (this->size.y - y) && alloc_size.x < this->size.x) {
		this->shelves.push_back({
			.size = {this->size.x, alloc_size.y},
			.y    = y,
			.free = this->size.x,
		});
		return shelf_alloc(*this, this->shelves.back(), alloc_size, id);
	}

	return -1;
}

const Allocation &ShelfAllocator::get(i32 id) const { return this->allocations.at(id); }

void ShelfAllocator::ref(i32 id)
{
	auto &alloc = this->allocations.at(id);
	alloc.refcount += 1;
}

bool ShelfAllocator::unref(i32 id)
{
	auto &alloc = this->allocations.at(id);
	alloc.refcount -= 1;
	if (alloc.refcount <= 0) {
		this->freelist.push_back({.alloc = alloc, .capacity = alloc.size});
		this->allocations.erase(id);
		return true;
	}
	return false;
}
