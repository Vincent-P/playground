#pragma once
#include <exo/maths/u128.h>
#include <exo/serialization/serializer.h>

namespace exo
{
inline void serialize(Serializer &serializer, u128 &value)
{
	if (serializer.is_writing) {
		u64 val0 = 0;
		u64 val1 = 0;
		u128_to_u64(value, &val0, &val1);
		serialize(serializer, val0);
		serialize(serializer, val1);
	} else {
		u64 val0 = 0;
		u64 val1 = 0;
		serialize(serializer, val0);
		serialize(serializer, val1);
		value = u128_from_u64(val1, val0);
	}
}
} // namespace exo
