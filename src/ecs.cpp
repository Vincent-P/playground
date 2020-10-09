#include "ecs.hpp"

#include <algorithm>
#include <cstring>
#include <doctest.h>

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

    auto previous_size = storage.size;

    storage.size += 1;

    usize total_size = storage.size * component_storage.component_size;

    // reserve enough space to add a component
    if (total_size > component_storage.data.size())
    {
        component_storage.data.reserve(total_size - component_storage.data.size() + 1);
        component_storage.data.resize(total_size);
    }

    auto *dst = &component_storage.data[previous_size * component_storage.component_size];

    std::memcpy(dst, data, len);
}
} // namespace impl

World::World()
{
    archetypes.root = archetypes.archetype_storages.add({});
}

TEST_SUITE("ECS")
{
    TEST_CASE("Archetypes")
    {
        struct Transform
        {
            uint a;
        };

        struct Position
        {
            uint a;
        };

        struct Rotation
        {
            uint a;
        };

        auto archetype_tp  = impl::create_archetype<Transform, Position>();
        auto archetype_tpr = impl::create_archetype<Transform, Position, Rotation>();

        CHECK(archetype_tp.size() == 2);
        CHECK(archetype_tpr.size() == 3);
        CHECK(archetype_tp[0] == archetype_tpr[0]);
        CHECK(archetype_tp[1] == archetype_tpr[1]);
    }

    TEST_CASE("Entity")
    {
        struct Transform
        {
            uint a{0};
            bool operator==(const Transform &other) const = default;
        };

        World world{};
        auto my_entity            = world.create_entity(Transform{42});
        auto *my_entity_transform = world.get_component<Transform>(my_entity);

        CHECK(my_entity_transform != nullptr);
        CHECK(*my_entity_transform == Transform{.a = 42});
    }
}

} // namespace my_app::ECS
