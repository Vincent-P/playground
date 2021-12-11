#include "assets/subscene.h"
#include "assets/asset_constructors.h"

static int subscene_ctor = global_asset_constructors().add_constructor("SBSC", &SubScene::create);

Asset *SubScene::create()
{
    return new SubScene();
}

void SubScene::serialize(Serializer& serializer)
{
    serializer.serialize(*this);
}

template <>
void Serializer::serialize<SubScene>(SubScene &data)
{
    const char *id = "SBSC";
    serialize(id);
    serialize(static_cast<Asset &>(data));
    serialize(data.roots);
    serialize(data.transforms);
    serialize(data.meshes);
    serialize(data.names);
    serialize(data.children);
}
