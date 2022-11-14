#pragma once

#include "exo/serialization/serializer.h"
#include "exo/string.h"

namespace exo
{
inline void serialize(Serializer &serializer, exo::String &data)
{
	usize len = 0;
	if (serializer.is_writing) {
		len = data.size();
		serialize(serializer, len);
		serializer.write_bytes(data.data(), len);
	} else {
		len = 0;
		serialize(serializer, len);
		data.resize(len);
		serializer.read_bytes(data.data(), len);
	}
}
} // namespace exo
