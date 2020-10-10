#include "ecs.hpp"

#include <algorithm>
#include <cstring>
#include <doctest.h>
#include <iostream>

namespace my_app::ECS
{

namespace impl
{

ArchetypeH find_or_create_archetype_storage_removing_component(Archetypes &graph, ArchetypeH entity_archetype,
                                                               ComponentId component_type)
{
    auto *entity_storage = graph.archetype_storages.get(entity_archetype);

    auto &edges = entity_storage->edges;
    // Add links if there is not enough space for the component_type
    if (component_type >= edges.size())
    {
        edges.reserve(component_type - edges.size() + 1);
        while (component_type >= edges.size())
        {
            edges.emplace_back();
        }
    }

    auto &next_h = edges[component_type].remove;

    // create a new storage if needed
    if (!next_h.is_valid())
    {
        next_h = graph.archetype_storages.add({});
        entity_storage = graph.archetype_storages.get(entity_archetype); // pointer was invalidated because of add()

        auto *new_storage = graph.archetype_storages.get(next_h);

        // The new archetype type is the same as entity type without the component that we are removing
        new_storage->type = entity_storage->type;
        auto new_end      = std::remove(std::begin(new_storage->type), std::end(new_storage->type), component_type);
        (void)(new_end);

        // Add links if there is not enough space for the component_type
        new_storage->edges.reserve(component_type - new_storage->edges.size() + 1);
        while (component_type >= new_storage->edges.size())
        {
            new_storage->edges.emplace_back();
        }
        new_storage->edges[component_type].add = entity_archetype;

        new_storage->components.resize(new_storage->type.size());
        for (usize i_component = 0; i_component < new_storage->components.size(); i_component++)
        {
            new_storage->components[i_component].component_size = 0; // TODO find actual size
        }
    }

    return next_h;
}

ArchetypeH find_or_create_archetype_storage_adding_component(Archetypes &graph, ArchetypeH entity_archetype,
                                                             ComponentId component_type)
{
    auto *entity_storage = graph.archetype_storages.get(entity_archetype);

    auto &edges = entity_storage->edges;
    // Add links if there is not enough space for the component_type
    if (component_type >= edges.size())
    {
        edges.reserve(component_type - edges.size() + 1);
        while (component_type >= edges.size())
        {
            edges.emplace_back();
        }
    }

    auto &next_h = edges[component_type].add;

    // create a new storage if needed
    if (!next_h.is_valid())
    {
        next_h = graph.archetype_storages.add({});
        entity_storage = graph.archetype_storages.get(entity_archetype); // pointer was invalidated because of add()

        auto *new_storage = graph.archetype_storages.get(next_h);

        // The new archetype type is the same as entity type without the component that we are removing
        new_storage->type = entity_storage->type;
        new_storage->type.push_back(component_type);

        // Add links if there is not enough space for the component_type
        new_storage->edges.reserve(component_type - new_storage->edges.size() + 1);
        while (component_type >= new_storage->edges.size())
        {
            new_storage->edges.emplace_back();
        }
        new_storage->edges[component_type].remove = entity_archetype;

        new_storage->components.resize(new_storage->type.size());
        for (usize i_component = 0; i_component < new_storage->components.size(); i_component++)
        {
            new_storage->components[i_component].component_size = 0; // TODO find actual size
        }
    }

    return next_h;
}

ArchetypeH find_or_create_archetype_storage_from_root(Archetypes &graph, const Archetype &type)
{
    ArchetypeH current_archetype = graph.root;
    // succesively add components from the root
    for (auto component_type : type)
    {
        current_archetype = find_or_create_archetype_storage_adding_component(graph, current_archetype, component_type);
    }
    return current_archetype;
}

usize add_entity(ArchetypeStorage &storage, EntityId entity)
{
    usize row = storage.entity_ids.size();
    storage.entity_ids.push_back(entity);
    return row;
}

void add_component(ArchetypeStorage &storage, usize i_component, void *data, usize len)
{

    auto &component_storage = storage.components[i_component];

    // TODO: Remove this hack to fill the component_size correctly
    assert(component_storage.component_size == 0 || component_storage.component_size == len);
    if (component_storage.component_size == 0)
    {
        component_storage.component_size = len;
    }

    usize total_size = (storage.size+1) * component_storage.component_size;

    // reserve enough space to add a component
    if (total_size > component_storage.data.size())
    {
        component_storage.data.reserve(total_size - component_storage.data.size() + 1);
        while (component_storage.data.size() < total_size)
        {
            component_storage.data.push_back(0);
        }
    }

    auto *dst = &component_storage.data[storage.size * component_storage.component_size];

    std::memcpy(dst, data, len);
}
} // namespace impl


ComponentId create_component(EntityIndex &entity_index, Archetypes &archetypes, ComponentId component_id, usize component_size)
{
    EntityId new_entity = component_id;
    auto archetype      = {ComponentId(family::type<InternalComponent>())};

    // find or create a new bucket for this archetype
    auto storage_h = impl::find_or_create_archetype_storage_from_root(archetypes, archetype);
    auto &storage  = *archetypes.archetype_storages.get(storage_h);

    // add the entity to the entity array
    auto row = impl::add_entity(storage, new_entity);

    impl::add_component(storage, 0, &component_size, sizeof(usize));
    storage.size++;

    // put the entity record in the entity index
    entity_index[new_entity] = EntityRecord{.archetype = storage_h, .row = row};

    return new_entity;
}

template <typename Component>
ComponentId create_component(EntityIndex &entity_index, Archetypes &archetypes)
{
    auto component_id = create_component(entity_index, archetypes, family::type<Component>(), sizeof(Component));
    assert(component_id.raw == family::type<Component>());
    return component_id;
}

World::World()
{
    archetypes.root = archetypes.archetype_storages.add({});

    // bootstrap the InternalComponent component
    auto internal_component = create_component<InternalComponent>(entity_index, archetypes);
    auto internal_id = create_component<InternalId>(entity_index, archetypes);

    add_component(internal_component, InternalId{"InternalComponentComponent"});
    add_component(internal_id, InternalId{"InternalIdComponent"});
}

void World::display_ui(UI::Context &ctx)
{
    if (ctx.begin_window("ECS", true))
    {
        for (auto &[storage_h, storage] : archetypes.archetype_storages)
        {
            ImGui::Separator();
            ImGui::Text("Storage handle: %u", storage_h.value());
            ImGui::TextUnformatted("Archetype: [");
            ImGui::SameLine();
            for (usize i_type_id = 0; i_type_id < storage->type.size(); i_type_id++) {
                auto component_id = storage->type[i_type_id];

                auto *internal_id = get_component<InternalId>(component_id);
                if (internal_id)
                {
                    ImGui::SameLine();
                    ImGui::Text("%s", internal_id->name);
                }
                else
                {
                    ImGui::Text("Component #%zu", component_id.raw);
                }

                ImGui::SameLine();
                if (i_type_id < storage->type.size()-1) {
                    ImGui::TextUnformatted(",");
                    ImGui::SameLine();
                }
            }
            ImGui::TextUnformatted("]");
            ImGui::TextUnformatted("Entities:");
            for (usize i_entity = 0; i_entity < storage->entity_ids.size(); i_entity++)
            {
                auto entity = storage->entity_ids[i_entity];
                ImGui::Text("#%zu", entity.raw);

                auto *internal_id = get_component<InternalId>(entity);
                if (internal_id)
                {
                    ImGui::SameLine();
                    ImGui::Text("%s", internal_id->name);
                }

                auto *internal_component = get_component<InternalComponent>(entity);
                if (internal_component)
                {
                    ImGui::Text("  Component size: %zu", internal_component->size);
                }
            }
        }

        ctx.end_window();
    }
}

namespace test
{

struct Position
{
    uint a;
};

struct Rotation
{
    uint a;
};

struct Transform
{
    uint a{0};
    bool operator==(const Transform &other) const = default;
};

std::ostream& operator<<(std::ostream& os, const Transform &t)
{
    os << t.a;
    return os;
}

TEST_SUITE("ECS")
{
    TEST_CASE("Archetypes")
    {
        auto archetype_tp  = impl::create_archetype<Transform, Position>();
        auto archetype_tpr = impl::create_archetype<Transform, Position, Rotation>();

        CHECK(archetype_tp.size() == 2);
        CHECK(archetype_tpr.size() == 3);
        CHECK(archetype_tp[0] == archetype_tpr[0]);
        CHECK(archetype_tp[1] == archetype_tpr[1]);
    }

    TEST_CASE("Entity")
    {
        World world{};

        // Create an entity with a list of components
        auto my_entity            = world.create_entity(Transform{42});
        auto *my_entity_transform = world.get_component<Transform>(my_entity);

        CHECK(my_entity_transform != nullptr);
        CHECK(*my_entity_transform == Transform{.a = 42});

        // Remove a component from an entity
        world.remove_component<Transform>(my_entity);
        auto *my_removed_entity_transform = world.get_component<Transform>(my_entity);

        CHECK(my_removed_entity_transform == nullptr);

        // Add a component to an entity
        world.add_component(my_entity, Transform{43});
        auto *my_new_entity_transform = world.get_component<Transform>(my_entity);

        CHECK(my_new_entity_transform != nullptr);
        CHECK(*my_new_entity_transform == Transform{.a = 43});

        // Set the value of a component
        world.set_component(my_entity, Transform{34});
        auto *my_changed_entity_transform = world.get_component<Transform>(my_entity);

        CHECK(my_changed_entity_transform != nullptr);
        CHECK(*my_changed_entity_transform == Transform{.a = 34});
    }
}
}

} // namespace my_app::ECS
