#include "assets/mesh.h"
#include "assets/asset_constructors.h"

#include <exo/serializer.h>

static int mesh_ctor = global_asset_constructors().add_constructor("MESH", &Mesh::create);

Asset *Mesh::create()
{
    return new Mesh();
}

void Mesh::serialize(exo::Serializer& serializer)
{
    const char *id = "MESH";
    serializer.serialize(id);
    serializer.serialize(*static_cast<Asset*>(this));
    serializer.serialize(this->indices);
    serializer.serialize(this->positions);
    serializer.serialize(this->uvs);
    serializer.serialize(this->submeshes);
}

template<>
inline void exo::Serializer::serialize<SubMesh>(SubMesh &data)
{
    serialize(data.first_index);
    serialize(data.first_vertex);
    serialize(data.index_count);
    serialize(data.material);
}
