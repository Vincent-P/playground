#pragma once

#include "gameplay/component.h"
#include <assets/asset_id.h>

struct MeshComponent : SpatialComponent
{
	AssetId mesh_asset;
};
