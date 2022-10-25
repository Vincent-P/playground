#include "assets/mesh.h"

#include <exo/serialization/serializer.h>
#include <exo/serialization/u128_serializer.h>
#include <exo/serialization/uuid_serializer.h>

void Mesh::serialize(exo::Serializer &serializer)
{
	Asset::serialize(serializer);

	exo::serialize(serializer, this->indices_hash);
	exo::serialize(serializer, this->indices_byte_size);

	exo::serialize(serializer, this->positions_hash);
	exo::serialize(serializer, this->positions_byte_size);

	exo::serialize(serializer, this->uvs_hash);
	exo::serialize(serializer, this->uvs_byte_size);

	exo::serialize(serializer, this->submeshes);
}

void serialize(exo::Serializer &serializer, SubMesh &data)
{
	exo::serialize(serializer, data.first_index);
	exo::serialize(serializer, data.first_vertex);
	exo::serialize(serializer, data.index_count);
	exo::serialize(serializer, data.material);
}
