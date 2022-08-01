#pragma once

#include <exo/maths/matrices.h>
#include <exo/uuid.h>

#include "assets/asset.h"
#include "assets/asset_id.h"

// Hierarchy of entities made of meshes and transforms
REGISTER_ASSET_TYPE(SubScene, create_asset_id('SUBS'))
struct SubScene : Asset
{
	static Asset *create();
	const char   *type_name() const final { return "SubScene"; }
	void          serialize(exo::Serializer &serializer) final;

	// --
	Vec<u32> roots;

	// SoA nodes layout
	Vec<float4x4>     transforms;
	Vec<AssetId>      meshes;
	Vec<const char *> names;
	Vec<Vec<u32>>     children;
};
