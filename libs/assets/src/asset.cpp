#include "assets/asset.h"

#include "exo/serialization/serializer.h"
#include "exo/serialization/string_serializer.h"

#include "assets/asset_id.h"

void Asset::serialize(exo::Serializer &serializer)
{
	exo::serialize(serializer, this->uuid);
	exo::serialize(serializer, this->name);
	exo::serialize(serializer, this->dependencies);
}
