#pragma once

#include "gameplay/component.h"
#include <exo/os/uuid.h>

struct MeshComponent : SpatialComponent
{
    os::UUID mesh_asset;
    void show_inspector_ui() override;
};
