#pragma once
#include <exo/prelude.h>
#include <exo/maths/matrices.h>
#include <exo/maths/aabb.h>
#include <exo/collections/vector.h>
#include <cross/uuid.h>
#include <string>

struct LoadingContext;

enum struct ComponentState
{
    Unloaded,      // Constructed, all properties are set, resources arent loaded yet
    Loading,       //  Resources are still loading
    Loaded,        // All resources are loaded
    LoadingFailed, // One or more resources failed to load
    Initialized    // Allows to allocate (dealocate at shutdown) transient data
};

struct BaseComponent
{
    cross::UUID uuid;
    std::string name;

    virtual ~BaseComponent() {}

    virtual void load(LoadingContext &)       { state = ComponentState::Loaded; }
    virtual void unload(LoadingContext &)     { state = ComponentState::Unloaded; }
    virtual void initialize(LoadingContext &) { state = ComponentState::Initialized; }
    virtual void shutdown(LoadingContext &)   { state = ComponentState::Loaded; }

    // clang-format off
    constexpr bool is_unloaded() const        { return state == ComponentState::Unloaded; }
    constexpr bool is_loading() const         { return state == ComponentState::Loading; }
    constexpr bool is_loaded() const          { return state == ComponentState::Loaded; }
    constexpr bool has_loading_failed() const { return state == ComponentState::LoadingFailed; }
    constexpr bool is_initialized() const     { return state == ComponentState::Initialized; }
    // clang-format on

  protected:
    ComponentState state;
};

struct SpatialComponent : BaseComponent
{
    void set_local_transform(const float4x4 &new_transform);
    void set_local_bounds(const AABB &new_bounds);

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

    SpatialComponent *      parent;
    Vec<SpatialComponent *> children;
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
