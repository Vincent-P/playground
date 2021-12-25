#include "glyph_cache.h"

#include <exo/memory/scope_stack.h>
#include <exo/macros/assert.h>

#include <cstring>

namespace
{
static GlyphEntry *get_entry(GlyphCache &cache, u32 i_entry)
{
    ASSERT(i_entry < cache.entry_count);
    return cache.entries + i_entry;
}

static GlyphEntry *get_sentinel(GlyphCache &cache)
{
    return get_entry(cache, 0);
}

static u32 *get_slot(GlyphCache &cache, u32 glyph_id)
{
    const u32 i = glyph_id & cache.hash_mask;
    return cache.slots + i;
}

static u32 evict_least_recently_used(GlyphCache &cache)
{
    auto *sentinel = get_sentinel(cache);
    const u32 i_evict = sentinel->lru_prev;
    ASSERT(i_evict);

    auto *entry_to_evict = get_entry(cache, i_evict);

    // Remove the last LRU entry from the chain
    auto *prev_entry = get_entry(cache, entry_to_evict->lru_prev);
    sentinel->lru_prev = entry_to_evict->lru_prev;
    prev_entry->lru_next = 0;

    // Find the entry in its hash chain
    u32 *i_next = get_slot(cache, entry_to_evict->glyph_id);
    while (*i_next != i_evict)
    {
        ASSERT(*i_next);
        i_next = &get_entry(cache, *i_next)->next;
    }
    ASSERT(*i_next == i_evict);
    *i_next = entry_to_evict->next;

    // Push the slot to the head of the free list
    entry_to_evict->next = sentinel->next;
    sentinel->next = i_evict;

    return 0;
}
} // namespace


GlyphCache *GlyphCache::create(exo::ScopeStack &scope, GlyphCacheParams params)
{
    GlyphCache *result = scope.allocate<GlyphCache>();

    result->params = params;
    result->entry_count = params.entry_count;
    result->slot_size = params.hash_count;
    result->hash_mask = params.hash_count - 1;

    result->slots = reinterpret_cast<u32*>(scope.allocate(params.hash_count * sizeof(u32)));
    result->entries = reinterpret_cast<GlyphEntry*>(scope.allocate(params.entry_count * sizeof(GlyphEntry)));

    std::memset(result->slots, 0, params.hash_count * sizeof(u32));

    u32 x = 0;
    u32 y = 0;

    for (u32 i_entry = 0; i_entry < params.entry_count; i_entry += 1)
    {
        if (x >= params.glyph_per_row)
        {
            x = 0;
            y += 1;
        }

        result->entries[i_entry].glyph_id = 0;
        result->entries[i_entry].x        = x;
        result->entries[i_entry].y        = y;
        result->entries[i_entry].uploaded = false;
        result->entries[i_entry].lru_prev = 0;
        result->entries[i_entry].lru_next = 0;
        result->entries[i_entry].next     = (i_entry + 1) < params.entry_count ? (i_entry + 1) : 0;

        x += 1;
    }

    return result;
}

/*
  Init():
    slot = {0, 0, 0, ..., 0}
    entries = {.next = 1}, {.next = 2}, ..., {.next = size-1}, {.next = 0}

    first entry is the sentinel for the lru cache and entry free list
 */

GlyphEntry &GlyphCache::get_or_create(u32 glyph_id)
{
    GlyphEntry *result = nullptr;
    auto *sentinel = get_sentinel(*this);

    // Find the entry corresponding to the glyph id
    u32 *slot = get_slot(*this, glyph_id);
    u32 i_entry = *slot;
    while (i_entry)
    {
        auto *entry = get_entry(*this, i_entry);
        if (entry->glyph_id == glyph_id)
        {
            break;
        }
        i_entry = entry->next;
    }

    // Entry not found, insert it
    if (!i_entry)
    {
        i_entry = sentinel->next;

        // The cache is full, we need to evict LRU elements to make space
        if (!i_entry)
        {
            i_entry = evict_least_recently_used(*this);
        }

        ASSERT(i_entry);
        result = get_entry(*this, i_entry);
        result->glyph_id = glyph_id;
        result->lru_prev = 0;
        result->lru_next = 0;

        sentinel->next = result->next;
        result->next = *slot;
        *slot = i_entry;
    }
    else
    {
        result = get_entry(*this, i_entry);

        // Remove the entry from the LRU chain
        auto *prev = get_entry(*this, result->lru_prev);
        auto *next = get_entry(*this, result->lru_next);
        prev->lru_next = result->lru_next;
        next->lru_prev = result->lru_prev;
    }

    ASSERT(result && i_entry);
    // Push the entry to the top of the LRU chain
    result->lru_prev = 0;
    result->lru_next = sentinel->lru_next;
    sentinel->lru_next = i_entry;

    return *result;
}
