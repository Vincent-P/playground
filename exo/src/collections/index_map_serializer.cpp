#include "exo/collections/index_map_serializer.h"

namespace exo
{
void serialize(Serializer &serializer, IndexMap &data)
{
	serialize(serializer, data.capacity);
	serialize(serializer, data.size);

	if (!serializer.is_writing) {
		data.keys   = reinterpret_cast<u64 *>(malloc(data.capacity * sizeof(u64)));
		data.values = reinterpret_cast<u64 *>(malloc(data.capacity * sizeof(u64)));
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
