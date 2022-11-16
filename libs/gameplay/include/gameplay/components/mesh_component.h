#pragma once

#include "gameplay/component.h"
#include "assets/asset_id.h"

struct MeshComponent : SpatialComponent
{
	using Self  = MeshComponent;
	using Super = SpatialComponent;
	REFL_REGISTER_TYPE_WITH_SUPER("MeshComponent")

	AssetId mesh_asset;

	// --
	void load(LoadingContext &) final;
	void unload(LoadingContext &) final;
	void update_loading(LoadingContext &) final;
	void serialize(exo::Serializer &serializer) final;
};
