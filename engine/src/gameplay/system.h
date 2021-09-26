#pragma once

#include <exo/prelude.h>

enum struct UpdateStages
{
    PrePhysics,
    Physics,
    PostPhysics,
    Count
};


struct BaseSystem
{
    virtual void update() = 0;
    UpdateStages update_stage;
};
