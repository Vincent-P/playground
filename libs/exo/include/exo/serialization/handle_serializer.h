#pragma once
#include "exo/collections/handle.h"
#include "exo/serialization/serializer.h"

namespace exo
{
template <typename T>
void serialize(Serializer &serializer, Handle<T> &value)
{
	serialize(serializer, value.index);
	serialize(serializer, value.gen);
}
} // namespace exo
