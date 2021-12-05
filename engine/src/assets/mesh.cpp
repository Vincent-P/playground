#include "assets/mesh.h"

void Mesh::serialize(Serializer& serializer)
{
    serializer.serialize(*this);
}

template<>
void Serializer::serialize<Mesh>(Mesh &data)
{
    const char *id = "MESH";
    serialize(id);
    serialize(static_cast<Asset&>(data));
    serialize(data.indices);
    serialize(data.positions);
    serialize(data.uvs);
    serialize(data.submeshes);
}
