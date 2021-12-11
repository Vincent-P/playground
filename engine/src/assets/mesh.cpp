#include "assets/mesh.h"
#include "assets/asset_constructors.h"

static int mesh_ctor = global_asset_constructors().add_constructor("MESH", &Mesh::create);

Asset *Mesh::create()
{
    return new Mesh();
}

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
