#pragma once

#include "exo/collections/index_map.h"
#include "exo/serializer.h"

namespace exo
{
void serialize(Serializer &serializer, IndexMap &data);
}
