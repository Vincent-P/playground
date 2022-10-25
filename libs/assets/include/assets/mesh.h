#pragma once
#include <exo/collections/vector.h>
#include <exo/maths/u128.h>
#include <exo/maths/vectors.h>

#include "assets/asset.h"

struct Material;

struct SubMesh
{
	u32     first_index  = 0;
	u32     first_vertex = 0;
	u32     index_count  = 0;
	AssetId material     = {};

	inline bool operator==(const SubMesh &other) const = default;
};
void serialize(exo::Serializer &serializer, SubMesh &data);

struct Mesh : Asset
{
	using Self  = Mesh;
	using Super = Asset;
	REFL_REGISTER_TYPE_WITH_SUPER("Mesh")

	exo::u128 indices_hash;
	usize     indices_byte_size;

	exo::u128 positions_hash;
	usize     positions_byte_size;

	exo::u128 uvs_hash;
	usize     uvs_byte_size;

	Vec<SubMesh> submeshes;

	// --
	void serialize(exo::Serializer &serializer) final;
};
