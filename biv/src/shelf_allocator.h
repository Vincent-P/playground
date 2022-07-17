#pragma once
#include <exo/collections/map.h>
#include <exo/collections/pool.h>
#include <exo/collections/vector.h>
#include <exo/maths/vectors.h>

// Simple implementation in JS: https://github.com/mapbox/shelf-pack
struct Allocation
{
	int2 pos      = int2(0);
	int2 size     = int2(0);
	i32  id       = 0;
	i32  refcount = 0;
};

struct FreeAllocation
{
	Allocation alloc    = {};
	int2       capacity = int2(0);
};

struct Shelf
{
	int2 size = int2(0, 0);
	i32  y    = 0;
	i32  free = 0;
};

struct ShelfAllocator
{
	int2 size = int2(0, 0);
	i32  gen  = 0;

	Vec<Shelf>                shelves;
	exo::Map<i32, Allocation> allocations;
	Vec<FreeAllocation>       freelist;

	i32               alloc(int2 alloc_size);
	const Allocation &get(i32 id) const;

	void ref(i32 id);
	// Returns true if the alloc has been freed
	bool unref(i32 id);
};
