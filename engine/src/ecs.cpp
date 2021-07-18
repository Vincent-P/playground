#include "ecs.h"
#include "ui.h"

#include <exo/logger.h>

#include <array>
#include <algorithm>
#include <cstring>
#if defined(ENABLE_DOCTEST)
#include <doctest.h>
#endif
#include <imgui/imgui.h>
#include <iostream>
#include <fmt/format.h>

namespace ECS
{

u64 family::identifier() noexcept
{
    static u64 value = 0;
    return value++;
}

std::string to_string(EntityId entity_id)
{
    u64 id = entity_id.id;
    u64 is_component = entity_id.is_component;
    return fmt::format("{{ id: {}, is_component: {}, raw: {} }}", id, is_component, entity_id.raw);
}

namespace impl
{

/// --- Archetype impl

Option<usize> get_component_idx(const Archetype &type, ComponentId component_id)
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
    if (component_type.id >= edges.size())
    {
        edges.reserve(component_type.id - edges.size() + 1);
        while (component_type.id >= edges.size())
        {
            edges.emplace_back();
        }
    }

    auto &next_h = edges[component_type.id].remove;

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
        new_storage->edges.reserve(component_type.id - new_storage->edges.size() + 1);
        while (component_type.id >= new_storage->edges.size())
        {
            new_storage->edges.emplace_back();
        }
        new_storage->edges[component_type.id].add = entity_archetype;

        new_storage->components.resize(new_storage->type.size());
        for (auto &component : new_storage->components)
        {
            component.component_size = 0; // TODO find actual size
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
    if (component_type.id >= edges.size())
    {
        edges.reserve(component_type.id - edges.size() + 1);
        while (component_type.id >= edges.size())
        {
            edges.emplace_back();
        }
    }

    auto &next_h = edges[component_type.id].add;

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
        new_storage->edges.reserve(component_type.id - new_storage->edges.size() + 1);
        while (component_type.id >= new_storage->edges.size())
        {
            new_storage->edges.emplace_back();
        }
        new_storage->edges[component_type.id].remove = entity_archetype;

        new_storage->components.resize(new_storage->type.size());
        for (auto &component : new_storage->components)
        {
            component.component_size = 0; // TODO find actual size
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
        component_storage.data.reserve(total_size - component_storage.data.size());
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
    for (auto old_component_id : old_storage.type)
    {
        auto &component_storage = old_storage.components[i_old_component++];
        void *src               = &component_storage.data[old_row * component_storage.component_size];

        auto i_new_component = get_component_idx(new_storage.type, old_component_id).value();
        add_component_to_storage(new_storage, i_new_component, src, component_storage.component_size);
    }

    // add the new component
    auto i_new_component = get_component_idx(new_storage.type, component_id).value();
    add_component_to_storage(new_storage, i_new_component, component_data, component_size);

    new_storage.size += 1; // /!\ DO THIS AFTER add_component

    /// --- Remove from previous storage
    Option<EntityId> swapped_entity;
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
    for (auto new_component_id : new_storage.type)
    {
        auto i_old_component    = *get_component_idx(old_storage.type, new_component_id);
        auto &component_storage = old_storage.components[i_old_component];
        void *src               = &component_storage.data[old_row * component_storage.component_size];
        usize component_size    = component_storage.component_size;

        add_component_to_storage(new_storage, i_new_component++, src, component_size);
    }

    new_storage.size += 1; // /!\ DO THIS AFTER add_component

    // remove from previous storage
    Option<EntityId> swapped_entity;
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

    return std::ranges::any_of(archetype_storage.type, [&](ComponentId component_id) {return component_id == component;});
}

void *get_component(World &world, EntityId entity, ComponentId component_id)
{
    // assert(world.entity_index.contains(entity));
    if (!world.entity_index.contains(entity))
    {
        logger::error("ECS: The world does not contain the entity {}\n", to_string(entity));
        return nullptr;
    }

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

/// --- Public API

World::World()
{
    archetypes.root = archetypes.archetype_storages.add({});

    // bootstrap the InternalComponent component
    auto internal_component = create_entity_internal(EntityId::component<InternalComponent>(), InternalComponent{sizeof(InternalComponent)});
    auto internal_id        = create_entity_internal(EntityId::component<InternalId>(), InternalComponent{sizeof(InternalId)});

    add_component(internal_component, InternalId{"InternalComponentComponent"});
    add_component(internal_id, InternalId{"InternalIdComponent"});

    singleton = create_entity("World");
}

void World::display_ui(UI::Context &ctx)
{
    if (ctx.begin_window("ECS"))
    {
        if (ImGui::CollapsingHeader("Archetypes"))
        {
            usize entity_count = 0;
            usize component_memory = 0;

            for (auto &[storage_h, storage] : archetypes.archetype_storages)
            {
                ImGui::Separator();
                ImGui::Text("Storage handle: %u", storage_h.value());
                ImGui::TextUnformatted("Archetype: [");
                ImGui::SameLine();
                for (usize i_type_id = 0; i_type_id < storage->type.size(); i_type_id++)
                {
                    const auto component_id = storage->type[i_type_id];

                    const auto *internal_id = get_component<InternalId>(component_id);
                    if (internal_id)
                    {
                        ImGui::SameLine();
                        ImGui::Text("%s", internal_id->tag);
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
                for (auto entity : storage->entity_ids)
                {
                    ImGui::Text("#%zu", entity.raw);

                    if (const auto *internal_id = get_component<InternalId>(entity))
                    {
                        ImGui::SameLine();
                        ImGui::Text("%s", internal_id->tag);
                    }

                    if (const auto *internal_component = get_component<InternalComponent>(entity))
                    {
                        ImGui::Text("  Component size: %zu", internal_component->size);
                    }
                }

                usize total_archetype_size = 0;
                for (const auto &component_storage : storage->components)
                {
                    total_archetype_size += component_storage.component_size;
                }
                total_archetype_size *= storage->size;

                component_memory += total_archetype_size;
                entity_count += storage->size;
            }

            ImGui::Separator();
            ImGui::Text("Total component size: %zu", component_memory);
            ImGui::Text("Entity count: %zu", entity_count);
        }

        if (ImGui::CollapsingHeader("Entities"))
        {
            for (const auto &[entity_id, entity_record] : entity_index)
            {
                // components are entities for the world
                if (has_component<InternalComponent>(entity_id)) { continue; }

                ImGui::Text("#%zu", entity_id.raw);
                if (const auto *internal_id = get_component<InternalId>(entity_id))
                {
                    ImGui::SameLine();
                    ImGui::Text("%s", internal_id->tag);
                }
            }
        }

        ctx.end_window();
    }
}

#if defined (ENABLE_DOCTEST)
namespace test
{
struct Position
{
    uint a = 0;
    bool operator==(const Position &other) const = default;
    static const char *type_name() { return "Position"; }
    void display_ui() {}
};

struct Rotation
{
    uint a = 0;
    bool operator==(const Rotation &other) const = default;
    static const char *type_name() { return "Rotation"; }
    void display_ui() {}
};

struct Transform
{
    uint a = 0;
    bool operator==(const Transform &other) const = default;
    static const char *type_name() { return "Transform"; }
    void display_ui() {}
};

std::ostream &operator<<(std::ostream &os, const Position &t)
{
    os << "Position{" << t.a << "}";
    return os;
}

std::ostream &operator<<(std::ostream &os, const Rotation &t)
{
    os << "Rotation{" << t.a << "}";
    return os;
}

std::ostream &operator<<(std::ostream &os, const Transform &t)
{
    os << "Transform{" << t.a << "}";
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

    TEST_CASE("Queries")
    {
        World world{};
        world.create_entity(Transform{42}, Position{21});
        world.create_entity(Transform{42});
        world.create_entity(Transform{42}, Rotation{21});

        world.create_entity(Transform{82}, Position{42});
        world.create_entity(Transform{84});
        world.create_entity(Transform{82}, Rotation{42});

        std::array<int, 256> values{};

        // count the transforms
        {
            values.fill(0);

            world.for_each<Transform>([&](const auto &transform) {
                int transform_value = transform.a;
                values[transform_value] += 1;
            });

            CHECK(values[42] == 3);
            CHECK(values[82] == 2);
            CHECK(values[84] == 1);
        }

        // count the position
        {
            values.fill(0);

            world.for_each<Position>([&](const auto &position) {
                int position_value = position.a;
                values[position_value] += 1;
            });

            CHECK(values[21] == 1);
            CHECK(values[42] == 1);
        }

        // count the rotation
        {
            values.fill(0);

            world.for_each<Rotation>([&](const auto &rotation) {
                int rotation_value = rotation.a;
                values[rotation_value] += 1;
            });

            CHECK(values[21] == 1);
            CHECK(values[42] == 1);
        }

    }
}
} // namespace test
#endif

} // namespace ECS
