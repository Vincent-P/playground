#include "exo/serialization/index_map_serializer.h"

#include "exo/profile.h"

namespace exo
{
void serialize(Serializer &serializer, IndexMap &data)
{
	serialize(serializer, data.capacity);
	serialize(serializer, data.size);

	if (!serializer.is_writing) {
		usize alloc_size = data.capacity * sizeof(u64);
		data.keys        = reinterpret_cast<u64 *>(malloc(alloc_size));
		data.values      = reinterpret_cast<u64 *>(malloc(alloc_size));
		EXO_PROFILE_MALLOC(data.keys, alloc_size);
		EXO_PROFILE_MALLOC(data.values, alloc_size);
	}

	if (serializer.is_writing) {
		serializer.write_bytes(data.keys, data.capacity * sizeof(u64));
		serializer.write_bytes(data.values, data.capacity * sizeof(u64));
	} else {
		serializer.read_bytes(data.keys, data.capacity * sizeof(u64));
		serializer.read_bytes(data.values, data.capacity * sizeof(u64));
	}
}
} // namespace exo
