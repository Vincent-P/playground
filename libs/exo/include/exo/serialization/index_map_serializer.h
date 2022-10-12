#pragma once

#include "exo/collections/index_map.h"
#include "exo/serialization/serializer.h"

namespace exo
{
void serialize(Serializer &serializer, IndexMap &data);
}
