#include "ecs.hpp"

#include <algorithm>
#include <cstring>
#include <doctest.h>
#include <iostream>

namespace my_app::ECS
{

namespace impl
{

/// --- Archetype impl

std::optional<usize> get_component_idx(Archetype type, ComponentId component_id)
{
    u32 component_idx = 0;
    for (auto type_id : type)
    {
        if (component_id == type_id)
        {
            return std::make_optional<usize>(component_idx);
            break;
        }
        component_idx++;
    }
    return std::nullopt;
}

/// --- ArchetypeStorage impl

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
        next_h         = graph.archetype_storages.add({});
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
        next_h         = graph.archetype_storages.add({});
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

usize add_entity_id_to_storage(ArchetypeStorage &storage, EntityId entity)
{
    usize row = storage.entity_ids.size();
    storage.entity_ids.push_back(entity);
    return row;
}

void add_component_to_storage(ArchetypeStorage &storage, usize i_component, void *data, usize len)
{

    auto &component_storage = storage.components[i_component];

    // TODO: Remove this hack to fill the component_size correctly
    assert(component_storage.component_size == 0 || component_storage.component_size == len);
    if (component_storage.component_size == 0)
    {
        component_storage.component_size = len;
    }

    usize total_size = (storage.size + 1) * component_storage.component_size;

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

void remove_entity_from_storage(ArchetypeStorage &storage, usize entity_row)
{
    auto entity_count = storage.entity_ids.size();

    // copy the last element to the old row
    if (entity_row < entity_count - 1)
    {
        storage.entity_ids[entity_row] = storage.entity_ids[entity_count - 1];
        for (usize i_component = 0; i_component < storage.type.size(); i_component++)
        {
            auto &component_storage = storage.components[i_component];
            auto stride             = component_storage.component_size;

            for (usize i_byte = 0; i_byte < stride; i_byte++)
            {
                component_storage.data[entity_row * stride + i_byte]
                    = component_storage.data[(entity_count - 1) * stride + i_byte];
            }
        }
    }

    storage.entity_ids.pop_back();
    for (usize i_component = 0; i_component < storage.type.size(); i_component++)
    {
        auto &component_storage = storage.components[i_component];
        for (usize i_byte = 0; i_byte < component_storage.component_size; i_byte++)
        {
            component_storage.data.pop_back();
        }
    }

    storage.size -= 1;
}

/// --- Components impl

void add_component(World &world, EntityId entity, ComponentId component_id, void *component_data, usize component_size)
{
    // get the entity information in its record
    auto &record = world.entity_index.at(entity);

    // find a new bucket for its new archetype
    auto new_storage_h
        = find_or_create_archetype_storage_adding_component(world.archetypes, record.archetype, component_id);
    auto &new_storage = *world.archetypes.archetype_storages.get(new_storage_h);

    // find the bucket corresponding to its old archetype
    auto &old_storage = *world.archetypes.archetype_storages.get(record.archetype);
    auto old_row      = record.row;

    // copy components to its new bucket
    auto new_row          = add_entity_id_to_storage(new_storage, entity);
    usize i_old_component = 0;
    for (auto component_id : old_storage.type)
    {
        auto &component_storage = old_storage.components[i_old_component++];
        void *src               = &component_storage.data[old_row * component_storage.component_size];
        usize component_size    = component_storage.component_size;

        auto i_new_component = *get_component_idx(new_storage.type, component_id);
        add_component_to_storage(new_storage, i_new_component, src, component_size);
    }

    // add the new component
    auto i_new_component = *get_component_idx(new_storage.type, component_id);
    add_component_to_storage(new_storage, i_new_component, component_data, component_size);

    new_storage.size += 1; // /!\ DO THIS AFTER add_component

    /// --- Remove from previous storage
    std::optional<EntityId> swapped_entity;
    if (old_row != old_storage.entity_ids.size() - 1)
    {
        swapped_entity = std::make_optional(old_storage.entity_ids.back());
    }

    remove_entity_from_storage(old_storage, old_row);

    /// --- Update entities' row
    record.row       = new_row;
    record.archetype = new_storage_h;

    if (swapped_entity)
    {
        auto &swapped_entity_record = world.entity_index.at(*swapped_entity);
        swapped_entity_record.row   = old_row;
    }
}

void remove_component(World &world, EntityId entity, ComponentId component_id)
{
    auto &record = world.entity_index.at(entity);

    // find a new bucket
    auto new_storage_h
        = find_or_create_archetype_storage_removing_component(world.archetypes, record.archetype, component_id);
    auto &new_storage = *world.archetypes.archetype_storages.get(new_storage_h);

    // find the bucket corresponding to its old archetype
    auto &old_storage = *world.archetypes.archetype_storages.get(record.archetype);
    auto old_row      = record.row;

    // copy components to a its new bucket
    auto new_row          = add_entity_id_to_storage(new_storage, entity);
    usize i_new_component = 0;
    for (auto component_id : new_storage.type)
    {
        auto i_old_component    = *get_component_idx(old_storage.type, component_id);
        auto &component_storage = old_storage.components[i_old_component];
        void *src               = &component_storage.data[old_row * component_storage.component_size];
        usize component_size    = component_storage.component_size;

        add_component_to_storage(new_storage, i_new_component++, src, component_size);
    }

    new_storage.size += 1; // /!\ DO THIS AFTER add_component

    // remove from previous storage
    std::optional<EntityId> swapped_entity;
    if (old_row != old_storage.entity_ids.size() - 1)
    {
        swapped_entity = std::make_optional(old_storage.entity_ids.back());
    }

    remove_entity_from_storage(old_storage, old_row);

    /// --- Update entities' row
    record.row       = new_row;
    record.archetype = new_storage_h;

    if (swapped_entity)
    {
        auto &swapped_entity_record = world.entity_index.at(*swapped_entity);
        swapped_entity_record.row   = old_row;
    }
}

void set_component(World &world, EntityId entity, ComponentId component_id, void *component_data, usize component_size)
{
    const auto &record      = world.entity_index.at(entity);
    auto &archetype_storage = *world.archetypes.archetype_storages.get(record.archetype);
    auto component_idx      = get_component_idx(archetype_storage.type, component_id);
    if (!component_idx)
    {
        add_component(world, entity, component_id, component_data, component_size);
        return;
    }

    auto &component_storage = archetype_storage.components[*component_idx];

    auto *dst = &component_storage.data[record.row * component_storage.component_size];
    std::memcpy(dst, component_data, component_size);
}

bool has_component(World &world, EntityId entity, ComponentId component)
{
    const auto &record            = world.entity_index.at(entity);
    const auto &archetype_storage = *world.archetypes.archetype_storages.get(record.archetype);
    for (auto component_id : archetype_storage.type)
    {
        if (component_id == component)
        {
            return true;
        }
    }
    return false;
}

void *get_component(World &world, EntityId entity, ComponentId component_id)
{
    // get the entity information in its record
    const auto &record = world.entity_index.at(entity);

    // find the bucket corresponding to its archetype
    auto &archetype_storage = *world.archetypes.archetype_storages.get(record.archetype);

    // each component is stored in a SoA so we need to find the right array
    auto component_idx = get_component_idx(archetype_storage.type, component_id);
    if (!component_idx)
    {
        return nullptr;
    }

    auto &component_storage = archetype_storage.components[*component_idx];

    // get the component data from the right array
    usize component_byte_idx = record.row * component_storage.component_size;
    return &component_storage.data[component_byte_idx];
}

} // namespace impl

/// --- Internal components

static ComponentId create_component(EntityIndex &entity_index, Archetypes &archetypes, ComponentId component_id,
                                    usize component_size)
{
    EntityId new_entity = component_id;
    auto archetype      = {ComponentId(family::type<InternalComponent>())};

    // find or create a new bucket for this archetype
    auto storage_h = impl::find_or_create_archetype_storage_from_root(archetypes, archetype);
    auto &storage  = *archetypes.archetype_storages.get(storage_h);

    // add the entity to the entity array
    auto row = impl::add_entity_id_to_storage(storage, new_entity);

    impl::add_component_to_storage(storage, 0, &component_size, sizeof(usize));
    storage.size++;

    // put the entity record in the entity index
    entity_index[new_entity] = EntityRecord{.archetype = storage_h, .row = row};

    return new_entity;
}

template <typename Component> static ComponentId create_component(EntityIndex &entity_index, Archetypes &archetypes)
{
    auto component_id = create_component(entity_index, archetypes, family::type<Component>(), sizeof(Component));
    assert(component_id.raw == family::type<Component>());
    return component_id;
}

/// --- Public API

World::World()
{
    archetypes.root = archetypes.archetype_storages.add({});

    // bootstrap the InternalComponent component
    auto internal_component = create_component<InternalComponent>(entity_index, archetypes);
    auto internal_id        = create_component<InternalId>(entity_index, archetypes);

    add_component(internal_component, InternalId{"InternalComponentComponent"});
    add_component(internal_id, InternalId{"InternalIdComponent"});
}

void World::display_ui(UI::Context &ctx)
{
    if (ctx.begin_window("ECS"))
    {
        for (auto &[storage_h, storage] : archetypes.archetype_storages)
        {
            ImGui::Separator();
            ImGui::Text("Storage handle: %u", storage_h.value());
            ImGui::TextUnformatted("Archetype: [");
            ImGui::SameLine();
            for (usize i_type_id = 0; i_type_id < storage->type.size(); i_type_id++)
            {
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
                if (i_type_id < storage->type.size() - 1)
                {
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

std::ostream &operator<<(std::ostream &os, const Transform &t)
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
} // namespace test

} // namespace my_app::ECS
