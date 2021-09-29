#include "gameplay/component.h"

void SpatialComponent::set_local_transform(const float4x4 &new_transform)
{
    local_transform = new_transform;

    world_transform = local_transform;
    SpatialComponent *p = parent;
    while (p != nullptr)
    {
        world_transform = p->local_transform * world_transform;
    }
}

void SpatialComponent::set_local_bounds(const AABB &new_bounds)
{
    local_bounds = new_bounds;
    // TODO: compute world_bounds
}
