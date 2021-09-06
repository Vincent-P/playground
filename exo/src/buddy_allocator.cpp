#include "exo/buddy_allocator.h"
#include "exo/numerics.h"
#include "exo/logger.h"
#include <cassert>
#include <bit>

static u32 total_size(u32 levels_count)
{
    return (1 << (levels_count - 1)) * LEAF_SIZE;
}

static u32 blocks_in_level(u32 level)
{
    return (1 << level);
}

static u32 block_size_of_level(u32 levels_count, u32 level)
{
    return total_size(levels_count) / blocks_in_level(level);
}

static u32 index_in_level_of(u32 offset, u32 levels_count, u32 level)
{
    return offset / block_size_of_level(levels_count, level);
}

static u32 block_unique_index(u32 offset, u32 levels_count, u32 level)
{
    return blocks_in_level(level) - 1 + index_in_level_of(offset, levels_count, level);
}

static u32 block_unique_index(u32 index_in_level, u32 level)
{
    return blocks_in_level(level) - 1 + index_in_level;
}

BuddyAllocator BuddyAllocator::create(usize capacity)
{
    // assert that capacity is a power of two
    assert(std::has_single_bit(capacity));

    // NOTE: std::countr_zero returns an int because "use an int unless you need something else" :|
    u32 log2_capacity = static_cast<u32>(std::countr_zero(capacity / LEAF_SIZE));

    BuddyAllocator allocator = {};
    allocator.capacity       = capacity;
    allocator.levels_count   = log2_capacity + 1;
    allocator.block_split.resize((1 << allocator.levels_count) - 1);
    allocator.block_free.resize((1 << allocator.levels_count) - 1);

    // Mark the root as free
    allocator.free_lists[0].push_front(0);
    allocator.block_free[block_unique_index(0, allocator.levels_count, 0)] = true;

    // logger::info("Buddy allocator ({})\n", total_size(allocator.levels_count));
    for (u32 i_level = 0; i_level < allocator.levels_count; i_level += 1)
    {
        // logger::info("[{}] size of level: {}\n", i_level, block_size_of_level(allocator.levels_count, i_level));
    }

    return allocator;
}

void BuddyAllocator::destroy()
{
    capacity     = 0;
    levels_count = 0;
    for (auto &list : free_lists)
    {
        list.clear();
    }
}

/**
if the list of free blocks at level n is empty
    allocate a block at level n-1 (using this algorithm)
    split the block into two blocks at level n
    insert the two blocks into the list of free blocks for level n
remove the first block from the list at level n and return it
**/
u32 BuddyAllocator::allocate_block(usize size, u32 level)
{
    assert(level < levels_count);
    if (free_lists[level].empty())
    {
        u32 first_block = allocate_block(size, level - 1);

        block_split[block_unique_index(first_block, levels_count, level - 1)] = true;

        u32 second_block                                                  = first_block + block_size_of_level(levels_count, level);
        block_free[block_unique_index(first_block, levels_count, level)]  = true;
        block_free[block_unique_index(second_block, levels_count, level)] = true;

        free_lists[level].push_front(second_block);
        free_lists[level].push_front(first_block);
    }

    u32 offset = free_lists[level].front();
    assert(block_free[block_unique_index(offset, levels_count, level)] == true);
    assert(block_split[block_unique_index(offset, levels_count, level)] == false);

    free_lists[level].pop_front();
    block_free[block_unique_index(offset, levels_count, level)] = false;
    return offset;
}

u32 BuddyAllocator::allocate(usize size)
{
    assert(size < capacity);

    u32 i_level = levels_count - 1;
    for (; i_level > 0; i_level -= 1)
    {
        if (size <= block_size_of_level(levels_count, i_level))
        {
            break;
        }
    }
    u32 offset = allocate_block(size, i_level);
    // logger::info("[Allocator] allocated {} bytes at offset {} level #{}\n", size, offset, i_level);
    allocated += block_size_of_level(levels_count, i_level);
    return offset;
}

u32 BuddyAllocator::find_block_level(u32 offset)
{
    u32 level = levels_count - 1;
    for (; level > 0; level -= 1)
    {
        if (block_split[block_unique_index(offset, levels_count, level)])
        {
            return level;
        }
    }
    return 0;
}

void BuddyAllocator::free_block(u32 offset, u32 level)
{
    u32 i_block = index_in_level_of(offset, levels_count, level);
    assert(block_free[block_unique_index(i_block, level)] == false);
    assert(block_split[block_unique_index(i_block, level)] == false);

    u32 i_buddy      = i_block % 2 == 0 ? i_block + 1 : i_block - 1;
    u32 buddy_offset = i_buddy * block_size_of_level(levels_count, level);

    // merge if the buddy is free
    if (block_free[block_unique_index(i_buddy, level)])
    {
        // Remove the buddy from the free list
        block_free[block_unique_index(i_buddy, level)] = false;
        std::erase(free_lists[level], buddy_offset);

        // Push the merged block to the free list of upper level
        u32 merged_offset                                                       = offset > buddy_offset ? buddy_offset : offset;
        block_free[block_unique_index(merged_offset, levels_count, level - 1)]  = true;
        block_split[block_unique_index(merged_offset, levels_count, level - 1)] = false;
        free_lists[level - 1].push_front(merged_offset);
    }
    else
    {
        block_free[block_unique_index(i_block, level)] = true;
        free_lists[level].push_front(offset);
    }
}

void BuddyAllocator::free(u32 offset)
{
    u32 level = find_block_level(offset);
    allocated -= block_size_of_level(levels_count, level);
    free_block(offset, level);
}
