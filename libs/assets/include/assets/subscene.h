#pragma once
#include <exo/maths/matrices.h>

#include "assets/asset.h"

// Hierarchy of entities made of meshes and transforms
struct SubScene : Asset
{
	using Self  = SubScene;
	using Super = Asset;
	REFL_REGISTER_TYPE_WITH_SUPER("SubScene")

	Vec<u32> roots;

	// SoA nodes layout
	Vec<float4x4>     transforms;
	Vec<AssetId>      meshes;
	Vec<const char *> names;
	Vec<Vec<u32>>     children;

	void serialize(exo::Serializer &serializer) final;
};
