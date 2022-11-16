#pragma once
#include "exo/collections/pool.h"
#include "exo/collections/vector.h"
#include "exo/maths/vectors.h"

// Simple implementation in JS: https://github.com/mapbox/shelf-pack

using AllocationId = Handle<struct Allocation>;
struct Allocation
{
	int2 pos      = int2(0);
	int2 size     = int2(0);
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

	Vec<Shelf>            shelves;
	exo::Pool<Allocation> allocations;
	Vec<FreeAllocation>   freelist;

	AllocationId      alloc(int2 alloc_size);
	const Allocation &get(AllocationId id) const;

	void ref(AllocationId id);
	// Returns true if the alloc has been freed
	bool unref(AllocationId id);
};
