#include "gameplay/entity_world.h"

#include "gameplay/entity.h"
#include "gameplay/component.h"
#include "gameplay/loading_context.h"
#include "gameplay/update_context.h"
#include "gameplay/update_stages.h"
#include "gameplay/system.h"

#include <imgui/imgui.h>
#include <exo/logger.h>

void EntityWorld::update(double delta_t)
{
    LoadingContext loading_context = {&system_registry};

    // -- Prepare entities
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

    // -- Prepare global systems

    for (auto &update_list : global_per_stage_update_list)
    {
        update_list.clear();
    }

    for (auto *global_system : system_registry.global_systems )
    {
        global_per_stage_update_list[global_system->update_stage].push_back(global_system);
    }

    for (auto &update_list : global_per_stage_update_list)
    {
        std::sort(update_list.begin(), update_list.end(), [](GlobalSystem *a, GlobalSystem *b){ return a->priority_per_stage[a->update_stage] > b->priority_per_stage[b->update_stage]; });
    }

    // --

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

        for (auto *system : global_per_stage_update_list[update_context.stage])
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
    root_entities.insert(new_entity);
    return new_entity;
}

void EntityWorld::set_parent_entity(Entity *entity, Entity *parent)
{
    entity->parent = parent;
    parent->attached_entities.push_back(entity);
    entity->attach_to_parent();
    parent->refresh_attachments();

    if (root_entities.contains(entity))
    {
        root_entities.erase(entity);
    }
}

void EntityWorld::destroy_entity(Entity *entity)
{
    entities.erase(entity);
    if (root_entities.contains(entity))
    {
        root_entities.erase(entity);
    }
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
    for (; i< size; i+= 1)
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

void EntityWorld::display_entity_tree_rec(Entity *entity, Entity* &selected)
{
    ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    node_flags |= entity == selected ? ImGuiTreeNodeFlags_Selected : 0;
    node_flags |= entity->attached_entities.empty() ? ImGuiTreeNodeFlags_Leaf : 0;

    bool node_open = ImGui::TreeNodeEx(entity, node_flags, "%s", entity->name.c_str());
    if (ImGui::IsItemClicked())
    {
        selected = entity;
    }

    if (node_open)
    {
        for (auto *child : entity->attached_entities)
        {
            display_entity_tree_rec(child, selected);
        }

        ImGui::TreePop();
    }
}

void EntityWorld::display_ui()
{
    ZoneScoped;
    static Entity* s_selected = nullptr;

    if (ImGui::Begin("Entities"))
    {
        for (auto *entity : root_entities)
        {
            display_entity_tree_rec(entity, s_selected);
        }
        ImGui::End();
    }

    if (ImGui::Begin("Inspector"))
    {
        if (s_selected)
        {
            ImGui::Text("Selected: %s", s_selected->name.c_str());
            for (auto *component : s_selected->components)
            {
                component->show_inspector_ui();
            }
        }

        ImGui::End();
    }
}
