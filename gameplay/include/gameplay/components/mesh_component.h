#pragma once

#include "gameplay/component.h"
#include <exo/uuid.h>

struct MeshComponent : SpatialComponent
{
	exo::UUID mesh_asset;
};
