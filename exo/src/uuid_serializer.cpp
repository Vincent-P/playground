#include "exo/uuid_serializer.h"

#include "exo/uuid_formatter.h"

static void write_uuid_string(u32 (&data)[4], char (&str)[exo::UUID::STR_LEN])
{
	std::memset(str, 0, exo::UUID::STR_LEN);
	fmt::format_to(str, "{:08x}-{:08x}-{:08x}-{:08x}", data[0], data[1], data[2], data[3]);
}

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