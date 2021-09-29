#include "gameplay/entity.h"

#include "gameplay/component.h"
#include "gameplay/system.h"
#include "gameplay/loading_context.h"
#include "gameplay/update_context.h"

void Entity::load(LoadingContext &ctx)
{
    ASSERT(state == EntityState::Unloaded);

    for (auto *component : components)
    {
        component->load(ctx);
        ASSERT(component->is_loaded() || component->is_loading());
        if (component->is_loaded())
        {
            component->initialize(ctx);
        }
    }

    state = EntityState::Loaded;
}

void Entity::unload(LoadingContext &ctx)
{
    ASSERT(state == EntityState::Loaded);

    for (auto *component : components)
    {
        component->unload(ctx);
        ASSERT(component->is_unloaded());
    }

    state = EntityState::Unloaded;
}

void Entity::activate(LoadingContext &ctx)
{
    ASSERT(state == EntityState::Loaded);
    state = EntityState::Activated;

    // if is spatial entity, update root

    for (auto *component : components)
    {
        if (component->is_initialized())
        {
            for (auto *system : local_systems)
            {
                system->register_component(component);
            }
            ctx.register_global_system(this, component);
        }
    }

    // generate system update list
    for (usize i_stage = 0; i_stage < static_cast<usize>(UpdateStages::Count); i_stage += 1)
    {
        auto stage = static_cast<UpdateStages>(i_stage);
        per_stage_update_list[stage].clear();

        for (auto *system : local_systems)
        {
            if (system->get_priority(stage) > 0.0)
            {
                per_stage_update_list[stage].push_back(system);
            }
        }

        auto comparator = [stage](const LocalSystem *lhs, const LocalSystem *rhs) {
            return lhs->get_priority(stage) > rhs->get_priority(stage);
        };

        std::sort(per_stage_update_list[stage].begin(), per_stage_update_list[stage].end(), comparator);
    }

    // attach entities

    ctx.register_entity_update(this);
}

void Entity::deactivate(LoadingContext &ctx)
{
    ASSERT(state == EntityState::Activated);

    // detach entities

    for (auto *component : components)
    {
        if (component->is_initialized())
        {
            for (auto *system : local_systems)
            {
                system->unregister_component(component);
            }
            ctx.unregister_global_system(this, component);
        }
    }

    ctx.unregister_entity_update(this);

    state = EntityState::Loaded;
}

void Entity::update_systems(const UpdateContext &ctx)
{
    for (auto *system : per_stage_update_list[ctx.stage])
    {
        system->update(ctx);
    }
}

void Entity::create_system_internal(LocalSystem *system)
{
    local_systems.push_back(system);
}

void Entity::destroy_system_internal(LocalSystem *system)
{
    usize i = 0;
    for (; i < local_systems.size(); i += 1)
    {
        if (system == local_systems[i])
        {
            break;
        }
    }

    // System not present in local sytems
    ASSERT(i < local_systems.size());

    if (i < local_systems.size() - 1)
    {
        std::swap(local_systems[i], local_systems[local_systems.size()-1]);
    }

    local_systems.pop_back();
}

void Entity::create_component_internal(BaseComponent *component)
{
    components.push_back(component);
}

void Entity::destroy_component_internal(BaseComponent *component)
{
    usize i = 0;
    for (; i < components.size(); i += 1)
    {
        if (component == components[i])
        {
            break;
        }
    }

    // Component not present in components
    ASSERT(i < components.size());

    if (i < components.size() - 1)
    {
        std::swap(components[i], components[components.size()-1]);
    }

    components.pop_back();
}
