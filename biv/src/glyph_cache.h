#pragma once
#include <exo/maths/numerics.h>
#include <exo/maths/vectors.h>

namespace exo { struct ScopeStack; }

struct GlyphEntry
{
    u32 glyph_id = 0;

    u32 x = 0;
    u32 y = 0;

    // user data
    bool uploaded = false;
    int2 glyph_top_left = {};
    int2 glyph_size     = {};

    // LRU chain
    u32 lru_prev = 0;
    u32 lru_next = 0;

    // hash chain (freelist for sentinel)
    u32 next = 0;
};

struct GlyphCacheParams
{
    u32 hash_count = 0;
    u32 entry_count = 0;
    u32 glyph_per_row = 0;
};

struct GlyphCache
{
    GlyphCache() = default;
    static GlyphCache *create(exo::ScopeStack &scope, GlyphCacheParams params);

    GlyphEntry &get_or_create(u32 codepoint);

    GlyphCacheParams params = {};
    u32 slot_size = 0; // needs to be a power of 2
    u32 hash_mask = 0; // used to truncate a hash to fit inside the slots array
    u32 entry_count = 0;

    u32 *slots = nullptr;
    GlyphEntry *entries = nullptr;
};
