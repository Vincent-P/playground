#pragma once

#include "exo/prelude.h"
#include <array>
#include <deque>
#include <vector>

inline constexpr u32 MAX_LEVELS = 32;
inline constexpr u32 LEAF_SIZE  = 128;

class BuddyAllocator
{
  public:
    static BuddyAllocator create(usize capacity);
    void                  destroy();

    u32  allocate(usize size);
    void free(u32 offset);

  private:
    u32  allocate_block(usize size, u32 level);
    void free_block(u32 offset, u32 level);
    u32  find_block_level(u32 offset);

    usize capacity     = 0;
    usize allocated    = 0;
    u32   levels_count = 0;
    std::array<std::deque<u32>, MAX_LEVELS> free_lists = {};
    std::vector<bool> block_split;
    std::vector<bool> block_free;
};
