#pragma once
#include "exo/collections/iterator_facade.h"
#include "exo/maths/numerics.h"
#include "exo/option.h"

/**
   A hash map using linear probing.
   The keys are 64-bit hash and the values are indices into an external array.
   This allows to make a non-templated hashmap that works with any type.
 **/

namespace exo
{
struct IndexMap;
struct IndexMapIterator : IteratorFacade<IndexMapIterator>
{

	struct Value
	{
		u64 hash;
		u64 index;
	};
	IndexMapIterator() = default;
	IndexMapIterator(const IndexMap *_map, u32 starting_index);

	Value dereference() const;
	void  increment();
	bool  equal_to(const IndexMapIterator &other) const;

private:
	void goto_next_valid();

	const IndexMap *map           = nullptr;
	u32             current_index = u32_invalid;
};

struct IndexMap
{
	static IndexMap with_capacity(u64 _capacity);
	~IndexMap();

	// Move-only struct
	IndexMap()                                 = default;
	IndexMap(const IndexMap &other)            = delete;
	IndexMap &operator=(const IndexMap &other) = delete;
	IndexMap(IndexMap &&other) noexcept;
	IndexMap &operator=(IndexMap &&other) noexcept;

	Option<u64> at(u64 hash);
	void        insert(u64 hash, u64 index);
	void        remove(u64 hash);

	IndexMapIterator begin() const { return IndexMapIterator(this, 0); }
	IndexMapIterator end() const { return IndexMapIterator(this, this->capacity); }

private:
	void check_growth();

	u64  *keys     = nullptr;
	u64  *values   = nullptr;
	usize capacity = 0;
	usize size     = 0;

	friend IndexMapIterator;
};

} // namespace exo
