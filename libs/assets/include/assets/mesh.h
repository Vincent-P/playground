#pragma once
#include <exo/collections/handle.h>
#include <exo/collections/vector.h>
#include <exo/maths/u128.h>
#include <exo/maths/vectors.h>

#include "assets/asset.h"
#include "assets/asset_id.h"

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

REGISTER_ASSET_TYPE(Mesh, create_asset_id('MESH'))
struct Mesh : Asset
{
	static Asset *create();
	const char   *type_name() const final { return "Mesh"; }
	void          serialize(exo::Serializer &serializer) final;

	// --
	exo::u128 indices_hash;
	usize     indices_byte_size;

	exo::u128 positions_hash;
	usize     positions_byte_size;

	exo::u128 uvs_hash;
	usize     uvs_byte_size;

	Vec<SubMesh> submeshes;
};
