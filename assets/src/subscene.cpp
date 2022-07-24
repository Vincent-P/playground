#include "assets/subscene.h"
#include "assets/asset_constructors.h"

#include <exo/serializer.h>

static int subscene_ctor = global_asset_constructors().add_constructor("SBSC", &SubScene::create);

Asset *SubScene::create() { return new SubScene(); }

void SubScene::serialize(exo::Serializer &serializer)
{
	const char *id = "SBSC";
	serializer.serialize(id);
	Asset::serialize(serializer);
	serializer.serialize(this->roots);
	serializer.serialize(this->transforms);
	serializer.serialize(this->meshes);
	serializer.serialize(this->names);
	serializer.serialize(this->children);
}
