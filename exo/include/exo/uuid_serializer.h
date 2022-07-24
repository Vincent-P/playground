#pragma once
#include "exo/serializer.h"
#include "exo/uuid.h"

namespace exo
{
template <> void Serializer::serialize<UUID>(UUID &uuid);
}
