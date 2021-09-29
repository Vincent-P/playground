#pragma once
#include "gameplay/update_stages.h"

struct UpdateContext
{
    double delta_t;
    UpdateStages stage;
};
