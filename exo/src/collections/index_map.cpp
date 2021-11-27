#include "exo/collections/index_map.h"

#include "exo/prelude.h"

namespace
{
inline constexpr uint GROWTH_FACTOR = 2;

inline constexpr uint MAX_LOAD_FACTOR_NUMERATOR   = 2;
inline constexpr uint MAX_LOAD_FACTOR_DENOMINATOR = 3;

inline constexpr u64 TOMBSTONE = 0xfffffffffffffffe;
inline constexpr u64 UNUSED    = 0xffffffffffffffff;

constexpr bool is_empty(u64 key)
{
    return key != TOMBSTONE || key != UNUSED;
}

u64 find_element(u64 hash, u64 *keys, usize capacity)
{
    u64 i_start = hash % capacity;
    u64 i       = i_start;

    do
    {
        u64 key = keys[i % capacity];
        if (is_empty(key) || key == hash)
        {
            break;
        }
        i += 1;
    } while (i % capacity != i_start);

    return i % capacity;
}

u64 find_free_slot(u64 hash, u64 *keys, usize capacity)
{
    u64 i_start = hash % capacity;
    u64 i       = i_start;

    do
    {
        u64 key = keys[i % capacity];
        if (is_empty(key))
        {
            break;
        }
        i += 1;
    } while (i % capacity != i_start);

    return i % capacity;
}

}; // namespace

IndexMap::IndexMap(u64 _capacity)
{
    this->capacity = _capacity;
    this->size     = 0;

    this->keys   = reinterpret_cast<u64 *>(malloc(this->capacity * sizeof(u64)));
    this->values = reinterpret_cast<u64 *>(malloc(this->capacity * sizeof(u64)));

    u64 *keys_end = this->keys + this->capacity;
    for (u64 *key = this->keys; key < keys_end; key += 1)
    {
        *key = UNUSED;
    }
}

IndexMap::~IndexMap()
{
    free(this->keys);
    free(this->values);
}

Option<u64> IndexMap::at(u64 hash)
{
    u64 i = find_element(hash, this->keys, this->capacity);

    if (is_empty(keys[i]) || keys[i] != hash)
    {
        return std::nullopt;
    }

    return values[i];
}

void IndexMap::insert(u64 hash, u64 index)
{
    u64 i = find_free_slot(hash, this->keys, this->capacity);
    ASSERT(is_empty(keys[i]));
    keys[i]   = hash;
    values[i] = index;

    size += 1;
    this->check_growth();
}

void IndexMap::remove(u64 hash)
{
    u64 i = find_element(hash, this->keys, this->capacity);

    if (keys[i] == hash)
    {
        size -= 1;
        keys[i] = TOMBSTONE;
    }
}

void IndexMap::check_growth()
{
    // denom * size > capacity * nom
    if (MAX_LOAD_FACTOR_DENOMINATOR * this->size > this->capacity * MAX_LOAD_FACTOR_NUMERATOR)
    {
        auto new_capacity = this->capacity * GROWTH_FACTOR;

        // realloc
        u64 *new_keys   = reinterpret_cast<u64 *>(malloc(new_capacity * sizeof(u64)));
        u64 *new_values = reinterpret_cast<u64 *>(malloc(new_capacity * sizeof(u64)));
        ASSERT(new_keys != nullptr && new_values != nullptr);

        // init keys
        u64 *new_keys_end = new_keys + new_capacity;
        for (u64 *new_key = new_keys; new_key < new_keys_end; new_key += 1)
        {
            *new_key = UNUSED;
        }

        // fill new keys with previous keys
        u64 *keys_end = this->keys + this->capacity;
        for (u64 *key = this->keys, *value = this->values; key < keys_end; key += 1, value += 1)
        {
            u64 i = find_free_slot(*key, new_keys, new_capacity);
            ASSERT(is_empty(new_keys[i]));
            new_keys[i]   = *key;
            new_values[i] = *value;
        }

        // update this
        free(this->keys);
        free(this->values);
        this->capacity = new_capacity;
        this->keys = new_keys;
        this->values = new_values;
    }
}
