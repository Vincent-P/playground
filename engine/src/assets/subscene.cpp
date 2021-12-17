#include "assets/subscene.h"
#include "assets/asset_constructors.h"

static int subscene_ctor = global_asset_constructors().add_constructor("SBSC", &SubScene::create);

Asset *SubScene::create()
{
    return new SubScene();
}

void SubScene::serialize(exo::Serializer& serializer)
{
    const char *id = "SBSC";
    serializer.serialize(id);
    serializer.serialize(*static_cast<Asset*>(this));
    serializer.serialize(this->roots);
    serializer.serialize(this->transforms);
    serializer.serialize(this->meshes);
    serializer.serialize(this->names);
    serializer.serialize(this->children);
}
