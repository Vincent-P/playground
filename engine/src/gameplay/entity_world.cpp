#include "gameplay/entity_world.h"

#include "gameplay/entity.h"
#include "gameplay/loading_context.h"
#include "gameplay/update_context.h"
#include "gameplay/update_stages.h"
#include "gameplay/system.h"

Entity* EntityWorld::create_entity()
{
    Entity *new_entity = new Entity();
    entities.insert(new_entity);
    return new_entity;
}

void EntityWorld::destroy_entity(Entity *entity)
{
    entities.erase(entity);
    delete entity;
}

void EntityWorld::update(double delta_t)
{
    LoadingContext loading_context = {&system_registry};

    for (auto *entity : entities)
    {
        if (entity->is_unloaded())
        {
            entity->load(loading_context);
        }
        else if (entity->is_loaded())
        {
            // all components should be initialized
            entity->activate(loading_context);
        }
    }

    UpdateContext update_context = {};
    update_context.delta_t = delta_t;
    for (usize i_stage = 0; i_stage < static_cast<usize>(UpdateStages::Count); i_stage += 1)
    {
        update_context.stage = static_cast<UpdateStages>(i_stage);

        // TODO: parallel for
        for (auto *entity : entities)
        {
            if (entity->is_active())
            {
                entity->update_systems(update_context);
            }
        }

        for (auto *system : system_registry.global_systems)
        {
            system->upate(update_context);
        }
    }
}
