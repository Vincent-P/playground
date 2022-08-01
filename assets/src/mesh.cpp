#include "assets/mesh.h"
#include "assets/asset_constructors.h"

#include <exo/serializer.h>
#include <exo/uuid_serializer.h>

static int mesh_ctor = global_asset_constructors().add_constructor(get_asset_id<Mesh>(), &Mesh::create);

Asset *Mesh::create() { return new Mesh(); }

void Mesh::serialize(exo::Serializer &serializer)
{
	const char *id = "MESH";
	exo::serialize(serializer, id);
	Asset::serialize(serializer);
	exo::serialize(serializer, this->indices);
	exo::serialize(serializer, this->positions);
	exo::serialize(serializer, this->uvs);
	exo::serialize(serializer, this->submeshes);
}

void serialize(exo::Serializer &serializer, SubMesh &data)
{
	exo::serialize(serializer, data.first_index);
	exo::serialize(serializer, data.first_vertex);
	exo::serialize(serializer, data.index_count);
	exo::serialize(serializer, data.material);
}
