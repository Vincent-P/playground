#include "assets/subscene.h"

#include <exo/serialization/serializer.h>
#include <exo/serialization/uuid_serializer.h>

void SubScene::serialize(exo::Serializer &serializer)
{
	Asset::serialize(serializer);
	exo::serialize(serializer, this->roots);
	exo::serialize(serializer, this->transforms);
	exo::serialize(serializer, this->meshes);
	exo::serialize(serializer, this->names);
	exo::serialize(serializer, this->children);
}
