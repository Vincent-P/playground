#include "exo/collections/pool.h"

#include "exo/hash.h"

namespace exo
{
usize _hash_handle(u32 index, u32 gen) { return hash_combine(u64(index), u64(gen)); }
} // namespace exo
