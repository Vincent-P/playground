#pragma once

#include "gameplay/component.h"
#include <cross/uuid.h>

struct MeshComponent : SpatialComponent
{
    cross::UUID mesh_asset;
};
