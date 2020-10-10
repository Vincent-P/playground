#pragma once
#include "base/pool.hpp"
#include "base/types.hpp"

#include <unordered_map>
#include <utility>
#include <vector>
#include <optional>

/**
   This ECS implementation is inspired by flecs (https://github.com/SanderMertens/flecs).
   Archetype-based ECS seems easier to implement than something like EnTT that uses sparse sets.
   Unity ECS also uses a Archetype-based implementation so it shouldn't be that bad.
   The main disadvantage of Archetypes is that adding/removing components is slow because every other components of that
   entity need to be memcpy'd.

   That implies that a relatively small number of small components should be used and that we shouldn't add/remove them
   a lot at runtime.
   That gives however very fast entity iterations and easy to implement grouping.
   It apparently scales well with very large number of entities.

   TODO: Maybe at some point this system will need to be rewritten using sparse sets like EnTT.
         Or maybe bitsets and registering every component type manually.
 **/

namespace my_app::ECS
{

namespace
{
// from EnTT, generates a unique unsigned integer per type
class family
{
    static u64 identifier() noexcept
    {
        static u64 value = 0;
        return value++;
    }

  public:
    template <typename> static u64 type() noexcept
    {
        static const u64 value = identifier();
        return value;
    }
};
}; // namespace

struct EntityId
{
    EntityId() = default;
    EntityId(u64 _raw)
        : raw(_raw)
    {
    }
    bool operator==(EntityId other) const
    {
        return raw == other.raw;
    }

    union
    {
        u64 raw;
    };

    operator u64() const
    {
        return raw;
    }
};
} // namespace my_app::ECS

namespace std
{
template <> struct hash<my_app::ECS::EntityId>
{
    std::size_t operator()(my_app::ECS::EntityId const &id) const noexcept
    {
        return std::hash<u64>{}(id.raw);
    }
};
} // namespace std

namespace my_app::ECS
{

using ComponentId = EntityId;
// An archetype is a collection of components
using Archetype = std::vector<ComponentId>;

// A vector of one component
struct ComponentStorage
{
    // chunks like Unity DOTS?
    std::vector<u8> data;   // buffer
    uint component_size{0}; // element size in bytes
};

struct ArchetypeStorage;
using ArchetypeH = Handle<ArchetypeStorage>;

// Each archetype is stored separately, and contains a SoA of components
struct ArchetypeStorage
{
    // A vector of component's type
    Archetype type;

    // List of entities whose components are stored in this archetype
    std::vector<EntityId> entity_ids;

    // List of components indexed by component type
    // (e.g components[family::type<MyComponent>()] contains a list of MyComponent)
    std::vector<ComponentStorage> components;
    usize size;

    struct Edge
    {
        ArchetypeH add;
        ArchetypeH remove;
    };

    // Edges to archetype if we add/remove the indexed component type
    // (e.g edges[family::type<MyComponent>()] contains a handle to "this archetype + MyComponent" and "this archetype -
    // MyComponent")
    std::vector<Edge> edges;
};

// All ArchetypeStorage are stored in a graph
struct Archetypes
{
    Pool<ArchetypeStorage> archetype_storages;
    ArchetypeH root;
};

// Metadata of an entity
struct EntityRecord
{
    // archetype of the entity
    ArchetypeH archetype;
    // index of the entity in the archetype storage
    usize row;
};

namespace impl
{
template <typename... ComponentTypes> Archetype create_archetype()
{
    Archetype result;
    (result.push_back(family::type<ComponentTypes>()), ...);
    return result;
}

// traverse the graph and returns or create the ArchetypeStorage matching the Archetype
ArchetypeH find_or_create_archetype_storage_removing_component(Archetypes &graph, ArchetypeH entity_archetype, ComponentId component_type);
ArchetypeH find_or_create_archetype_storage_adding_component(Archetypes &graph, ArchetypeH entity_archetype, ComponentId component_type);
ArchetypeH find_or_create_archetype_storage_from_root(Archetypes &graph, const Archetype &type);

usize add_entity(ArchetypeStorage &storage, EntityId entity);
void add_component(ArchetypeStorage &storage, usize i_component, void *data, usize len);

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

template <typename Component>
std::optional<usize> get_component_idx(Archetype type)
{
    return get_component_idx(type, family::type<Component>());
}
} // namespace impl

struct World
{
    World();

    /// --- Entities

    // Create an entity with a list of components
    template <typename... ComponentTypes> EntityId create_entity(ComponentTypes &&... components)
    {
        EntityId new_entity = 0;
        auto archetype      = impl::create_archetype<ComponentTypes...>();

        // find or create a new bucket for this archetype
        auto storage_h = impl::find_or_create_archetype_storage_from_root(archetypes, archetype);
        auto &storage  = *archetypes.archetype_storages.get(storage_h);

        // add the entity to the entity array
        auto row = impl::add_entity(storage, new_entity);

        // add the component to every component array, fold expression black magic...
        uint component_i = 0;
        ((impl::add_component(storage, component_i++, &components, sizeof(ComponentTypes)), storage.size++), ...);

        // put the entity record in the entity index
        entity_index[new_entity] = EntityRecord{.archetype = storage_h, .row = row};

        return new_entity;
    }

    /// --- Components

    // Remove a component from an entity
    template <typename Component> void remove_component(EntityId entity)
    {
        // get the entity information in its record
        auto &record = entity_index.at(entity);

        // find the bucket corresponding to its archetype
        auto &old_storage = *archetypes.archetype_storages.get(record.archetype);
        auto old_row      = record.row;

        // find a new bucket for its new archetype
        auto new_storage_h = impl::find_or_create_archetype_storage_removing_component(archetypes,
                                                                                       record.archetype,
                                                                                       family::type<Component>());
        auto &new_storage  = *archetypes.archetype_storages.get(new_storage_h);

        /// --- Copy components to a new storage

        // copy old_storage[component_id] to new_storage[component_id]
        auto new_row = impl::add_entity(new_storage, entity);
        usize i_new_component = 0;
        for (auto component_id : new_storage.type)
        {
            auto i_old_component = *impl::get_component_idx(old_storage.type, component_id);
            auto &component_storage = old_storage.components[i_old_component];
            void *src = &component_storage.data[old_row * component_storage.component_size];
            usize component_size = component_storage.component_size;

            impl::add_component(new_storage, i_new_component++, src, component_size);
        }
        new_storage.size += 1;

        /// --- Remove from previous storage
        auto entity_count = old_storage.entity_ids.size();

        // copy the last element to the old row
        if (old_row < entity_count - 1)
        {
            old_storage.entity_ids[old_row] = old_storage.entity_ids[entity_count - 1];
            for (usize i_component = 0; i_component < old_storage.type.size(); i_component++)
            {
                auto &component_storage = old_storage.components[i_component];
                auto stride             = component_storage.component_size;

                for (usize i_byte = 0; i_byte < stride; i_byte++)
                {
                    component_storage.data[old_row * stride + i_byte]
                        = component_storage.data[(entity_count - 1) * stride + i_byte];
                }
            }
        }

        // pop the last element to effectively remove the entity from the old storage
        auto *entity_to_update = old_storage.entity_ids.size() > 1 ? &old_storage.entity_ids.back() : nullptr;

        old_storage.entity_ids.pop_back();
        for (usize i_component = 0; i_component < old_storage.type.size(); i_component++)
        {
            auto &component_storage = old_storage.components[i_component];
            for (usize i_byte = 0; i_byte < component_storage.component_size; i_byte++)
            {
                component_storage.data.pop_back();
            }
        }
        old_storage.size -= 1;

        /// --- Update entities' row
        record.row = new_row;
        record.archetype = new_storage_h;

        if (entity_to_update)
        {
            auto &entity_to_update_record = entity_index.at(*entity_to_update);
            entity_to_update_record.row   = old_row;
        }
    }

    // Add a component to an entity, the entity SHOULD NOT already have that component
    template <typename Component> void add_component(EntityId entity, Component component)
    {
        // get the entity information in its record
        auto &record = entity_index.at(entity);

        // find the bucket corresponding to its archetype
        auto &old_storage = *archetypes.archetype_storages.get(record.archetype);
        auto old_row      = record.row;

        // find a new bucket for its new archetype
        auto new_storage_h = impl::find_or_create_archetype_storage_adding_component(archetypes,
                                                                                       record.archetype,
                                                                                       family::type<Component>());
        auto &new_storage  = *archetypes.archetype_storages.get(new_storage_h);

        /// --- Copy components to a new storage

        // copy old_storage[component_id] to new_storage[component_id]
        auto new_row = impl::add_entity(new_storage, entity);
        usize i_old_component = 0;
        for (auto component_id : old_storage.type)
        {
            auto &component_storage = old_storage.components[i_old_component++];
            void *src = &component_storage.data[old_row * component_storage.component_size];
            usize component_size = component_storage.component_size;

            auto i_new_component = *impl::get_component_idx(new_storage.type, component_id);
            impl::add_component(new_storage, i_new_component, src, component_size);
        }

        // add new component
        {
            auto i_new_component = *impl::get_component_idx<Component>(new_storage.type);
            impl::add_component(new_storage, i_new_component, &component, sizeof(Component));
        }

        new_storage.size += 1; // /!\ DO THIS AFTER add_component

        /// --- Remove from previous storage
        auto entity_count = old_storage.entity_ids.size();

        // copy the last element to the old row
        if (old_row < entity_count - 1)
        {
            old_storage.entity_ids[old_row] = old_storage.entity_ids[entity_count - 1];
            for (usize i_component = 0; i_component < old_storage.type.size(); i_component++)
            {
                auto &component_storage = old_storage.components[i_component];
                auto stride             = component_storage.component_size;

                for (usize i_byte = 0; i_byte < stride; i_byte++)
                {
                    component_storage.data[old_row * stride + i_byte]
                        = component_storage.data[(entity_count - 1) * stride + i_byte];
                }
            }
        }

        // pop the last element to effectively remove the entity from the old storage
        auto *entity_to_update = old_storage.entity_ids.size() > 1 ? &old_storage.entity_ids.back() : nullptr;

        old_storage.entity_ids.pop_back();
        for (usize i_component = 0; i_component < old_storage.type.size(); i_component++)
        {
            auto &component_storage = old_storage.components[i_component];
            for (usize i_byte = 0; i_byte < component_storage.component_size; i_byte++)
            {
                component_storage.data.pop_back();
            }
        }

        old_storage.size -= 1;

        /// --- Update entities' row
        record.row = new_row;
        record.archetype = new_storage_h;

        if (entity_to_update)
        {
            auto &entity_to_update_record = entity_index.at(*entity_to_update);
            entity_to_update_record.row   = old_row;
        }
    }

    // Set the value of a component or add it to an entity
    template <typename Component> void set_component(EntityId entity, Component component)
    {
        const auto &record = entity_index.at(entity);
        auto &archetype_storage = *archetypes.archetype_storages.get(record.archetype);
        auto component_idx     = impl::get_component_idx<Component>(archetype_storage.type);
        if (!component_idx)
        {
            add_component(entity, component);
            return;
        }

        auto &component_storage = archetype_storage.components[*component_idx];

        auto *dst = &component_storage.data[record.row * component_storage.component_size];
        std::memcpy(dst, &component, sizeof(Component));
    }

    // Get a component from an entity, returns nullptr if not found
    template <typename Component> Component *get_component(EntityId entity)
    {
        // get the entity information in its record
        const auto &record = entity_index.at(entity);

        // find the bucket corresponding to its archetype
        auto &archetype_storage = *archetypes.archetype_storages.get(record.archetype);

        // each component is stored in a SoA so we need to find the right array
        auto component_idx     = impl::get_component_idx<Component>(archetype_storage.type);
        if (!component_idx) {
            return nullptr;
        }

        auto &component_storage = archetype_storage.components[*component_idx];

        // get the component data from the right array
        usize component_byte_idx = record.row * component_storage.component_size;
        return reinterpret_cast<Component *>(&component_storage.data[component_byte_idx]);
    }

  private:
    // Metadata of entites
    std::unordered_map<EntityId, EntityRecord> entity_index;
    Archetypes archetypes;
};

}; // namespace my_app::ECS
