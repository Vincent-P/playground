#include "assets/subscene.h"
#include "assets/asset_constructors.h"

#include <exo/serializer.h>
#include <exo/uuid_serializer.h>

static int subscene_ctor = global_asset_constructors().add_constructor(get_asset_id<SubScene>(), &SubScene::create);

Asset *SubScene::create() { return new SubScene(); }

void SubScene::serialize(exo::Serializer &serializer)
{
	const char *id = "SBSC";
	exo::serialize(serializer, id);
	Asset::serialize(serializer);
	exo::serialize(serializer, this->roots);
	exo::serialize(serializer, this->transforms);
	exo::serialize(serializer, this->meshes);
	exo::serialize(serializer, this->names);
	exo::serialize(serializer, this->children);
}
