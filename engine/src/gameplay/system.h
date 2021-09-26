#pragma once

#include <exo/prelude.h>

struct Entity;
struct BaseComponent;

enum struct UpdateStages
{
    FrameStart,
    PrePhysics,
    Physics,
    PostPhysics,
    FrameEnd,
    Count
};


struct BaseSystem
{
    virtual void update() = 0;
    UpdateStages update_stage;
};

struct UpdateContext;
struct SystemRegistry;

struct GlobalSystem
{
    friend struct EntityWorld;

protected:
    virtual void initialize(const SystemRegistry &registry) {};
    virtual void shutdown() {};
    virtual void upate(const UpdateContext& ctx) {};

    // Called when a new component is activated (added to world)
    virtual void register_component(const Entity *entity, BaseComponent *component) = 0;

    // Called immediatly before a component is deactivated
    virtual void unregister_component(const Entity *entity, BaseComponent *component) = 0;
};
