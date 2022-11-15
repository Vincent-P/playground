#include "exo/serialization/uuid_serializer.h"
#include "exo//uuid.h"

namespace exo
{
void serialize(Serializer &serializer, UUID &uuid)
{
	serialize(serializer, uuid.data[0]);
	serialize(serializer, uuid.data[1]);
	serialize(serializer, uuid.data[2]);
	serialize(serializer, uuid.data[3]);

	if (serializer.is_writing == false) {
		write_uuid_string(uuid.data, uuid.str);
	}
}
} // namespace exo
