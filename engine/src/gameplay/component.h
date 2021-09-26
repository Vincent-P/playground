#pragma once
#include <exo/prelude.h>
#include <exo/maths/matrices.h>
#include <exo/maths/aabb.h>
#include <exo/collections/vector.h>
#include <string>

enum struct ComponentState
{
    Unloaded,
    Loading,
    Loaded,
    LoadingFailed,
    Initialized
};

struct BaseComponent
{
    virtual void load()   = 0;
    virtual void unload() = 0;
    virtual void initialize() = 0;
    virtual void shutdown()   = 0;

  protected:
    ComponentState state;
};

struct EntityComponent : BaseComponent
{
    u64         uid;
    std::string name;
    // empty component?
};

struct SpatialEntityComponent : EntityComponent
{
    void set_local_transform(const float4x4 &new_transform);
    void set_local_bounds(const float4x4 &new_bounds);

    // clang-format off
    inline const float4x4 &get_local_transform() const { return local_transform; }
    inline const AABB     &get_local_bounds() const    { return local_bounds; }
    inline const float4x4 &get_world_transform() const { return world_transform; }
    inline const AABB     &get_world_bounds() const    { return world_bounds; }
    // clang-format on

  private:
    float4x4 local_transform;
    AABB     local_bounds;
    float4x4 world_transform;
    AABB     world_bounds;

    SpatialEntityComponent *      parent;
    Vec<SpatialEntityComponent *> children;
};

/**
An entity can have only one spatial component root, so i guess a mesh component is a spatial entity component?
wait at 1h58 there are two skeletal mesh components so they cant be spatial components unless the hierarchy is not in the slide and the pistol is in fact a child of the human
From follow-up stream we can see some comopnents hierarchies:
capsule comp
  - mesh comp (torso)
  - mesh comp (legs)
  - mesh comp (hands)
  - mesh comp (head)
    - mesh comp (glasses)
    - mesh comp (hat)
  - mesh comp (feet)
  - mesh comp (arm)
  - mesh comp (backpack)
weapon comp
...


these mesh comp are probably skeletal mesh components?

so spatial components can be: physics colliders, (skeletal) meshes
**/
