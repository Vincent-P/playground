#pragma once

#include <exo/maths/matrices.h>
#include <exo/uuid.h>

#include "assets/asset.h"

// Hierarchy of entities made of meshes and transforms
struct SubScene : Asset
{
	static Asset *create();
	const char   *type_name() const final { return "SubScene"; }
	void          serialize(exo::Serializer &serializer) final;

	// --
	Vec<u32> roots;

	// SoA nodes layout
	Vec<float4x4>     transforms;
	Vec<exo::UUID>    meshes;
	Vec<const char *> names;
	Vec<Vec<u32>>     children;
};
