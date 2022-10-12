#pragma once
#include "exo/serialization/serializer.h"
#include "exo/uuid.h"

namespace exo
{
void serialize(Serializer &serializer, UUID &uuid);
}
