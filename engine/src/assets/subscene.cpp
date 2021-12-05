#include "assets/subscene.h"

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
