#include "gameplay/entity_world.h"

#include "gameplay/entity.h"
#include "gameplay/loading_context.h"
#include "gameplay/update_context.h"
#include "gameplay/update_stages.h"
#include "gameplay/system.h"

#include <imgui/imgui.h>

void EntityWorld::update(double delta_t)
{
    LoadingContext loading_context = {&system_registry};

    for (auto *entity : entities)
    {
        if (entity->is_unloaded())
        {
            entity->load(loading_context);
        }
        if (entity->is_loaded())
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
            system->update(update_context);
        }
    }

    // -- UI

    display_ui();
}


Entity* EntityWorld::create_entity(std::string_view name)
{
    Entity *new_entity = new Entity();
    new_entity->name = name;
    entities.insert(new_entity);
    return new_entity;
}

void EntityWorld::destroy_entity(Entity *entity)
{
    entities.erase(entity);
    delete entity;
}

void EntityWorld::create_system_internal(GlobalSystem *system)
{
    system_registry.global_systems.push_back(system);
}

void EntityWorld::destroy_system_internal(GlobalSystem *system)
{
    usize i = 0;
    usize size = system_registry.global_systems.size();
    for (; i < size; i += 1)
    {
        if (system_registry.global_systems[i] == system)
        {
            break;
        }
    }

    if (i < size - 1)
    {
        std::swap(system_registry.global_systems[i], system_registry.global_systems.back());
    }

    if (i < size)
    {
        system_registry.global_systems.pop_back();
    }
}

void EntityWorld::display_entity_tree_rec(Entity *entity)
{
    if (ImGui::TreeNode(entity, "%s", entity->name.c_str()))
    {
        for (auto *child : entity->attached_entities)
        {
            display_entity_tree_rec(child);
        }

        ImGui::TreePop();
    }
}

void EntityWorld::display_ui()
{
    if (ImGui::Begin("World"))
    {
        if (ImGui::TreeNode("Entities"))
        {
            for (auto *entity : entities)
            {
                display_entity_tree_rec(entity);
            }

            ImGui::TreePop();
        }
        ImGui::End();
    }
}
