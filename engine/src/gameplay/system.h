#pragma once

#include <exo/prelude.h>
#include <exo/collections/enum_array.h>

#include "gameplay/update_stages.h"

struct Entity;
struct BaseComponent;

struct SystemRegistry;
struct UpdateContext;
// required_update_priorities is a bitmask of update stages? + priority per stage
// so for example  where priority < 0 is does not contain stage

struct LocalSystem
{
    friend struct Entity;

    virtual ~LocalSystem() {}
    virtual void update(const UpdateContext &ctx) = 0;

    // Called when a new component is activated (added to world)
    virtual void register_component(BaseComponent *component) = 0;

    // Called immediatly before a component is deactivated
    virtual void unregister_component(BaseComponent *component) = 0;

    /**
       XXComponent *xx_component;
       Vec<YYComponent*> yy_components
     **/
    constexpr float get_priority(UpdateStages stage) const { return priority_per_stage[stage]; }

protected:
    UpdateStages update_stage = UpdateStages::FrameStart;
    EnumArray<float, UpdateStages> priority_per_stage = {};
};

struct GlobalSystem
{
    friend struct EntityWorld;
    friend struct LoadingContext;

    virtual ~GlobalSystem() {}

protected:
    virtual void initialize(const SystemRegistry &) {}
    virtual void shutdown() {}
    virtual void upate(const UpdateContext&) {}

    // Called when a new component is activated (added to world)
    virtual void register_component(const Entity *entity, BaseComponent *component) = 0;

    // Called immediatly before a component is deactivated
    virtual void unregister_component(const Entity *entity, BaseComponent *component) = 0;

    /**
       struct Record
       {
           BaseComponent *component;
       };
       map<EntityId, Record*> XXX_components;
    **/
};
