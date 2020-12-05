#pragma once
#include "base/pool.hpp"
#include "base/types.hpp"
#include "ui.hpp"

#include <optional>
#include <unordered_map>
#include <vector>

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
struct family
{
    static u64 identifier() noexcept
    {
        static u64 value = 0;
        return value++;
    }

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
    bool operator==(EntityId other) const { return raw == other.raw; }

    union
    {
        u64 raw;
    };

    operator u64() const { return raw; }
};
} // namespace my_app::ECS

namespace std
{
template <> struct hash<my_app::ECS::EntityId>
{
    std::size_t operator()(my_app::ECS::EntityId const &id) const noexcept { return std::hash<u64>{}(id.raw); }
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

    // Edges to archetype if we add/remove the indexed component type
    // (e.g edges[family::type<MyComponent>()] contains a handle to "this archetype + MyComponent" and "this archetype -
    // MyComponent")
    struct Edge
    {
        ArchetypeH add;
        ArchetypeH remove;
    };
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

using EntityIndex = std::unordered_map<EntityId, EntityRecord>;

/// --- Builtin Components

struct InternalComponent
{
    // size of the type of the component in bytes
    usize size;
};

struct InternalId
{
    const char *name;
};

struct World;

namespace impl
{
// Archetype
template <typename... ComponentTypes> Archetype create_archetype()
{
    constexpr usize component_count = sizeof...(ComponentTypes);
    Archetype result;
    result.resize(component_count);
    usize i = 0;
    ((result[i++] = family::type<ComponentTypes>()), ...);
    return result;
}

inline std::tuple<bool, std::vector<u32>> archetype_contains(const Archetype &query, const Archetype &archetype)
{
    std::vector<u32> found(query.size(), u32_invalid);

    if (query.size() > archetype.size())
    {
        return std::make_tuple(false, found);
    }

    for (usize i_archetype = 0; i_archetype < archetype.size(); i_archetype++)
    {
        auto component_id = archetype[i_archetype];
        for (usize i_query = 0; i_query < query.size(); i_query++)
        {
            if (component_id == query[i_query])
            {
                found[i_query] = i_archetype;
                break;
            }
        }
    }

    for (const auto component_found : found)
    {
        if (component_found == u32_invalid)
        {
            return std::make_tuple(false, found);
        }
    }

    return std::make_tuple(true, found);
}

std::optional<usize> get_component_idx(Archetype type, ComponentId component_id);

// ArchetypeStorage
// traverse the graph and returns or create the ArchetypeStorage matching the Archetype
ArchetypeH find_or_create_archetype_storage_removing_component(Archetypes &graph, ArchetypeH entity_archetype,
                                                               ComponentId component_type);
ArchetypeH find_or_create_archetype_storage_adding_component(Archetypes &graph, ArchetypeH entity_archetype,
                                                             ComponentId component_type);
ArchetypeH find_or_create_archetype_storage_from_root(Archetypes &graph, const Archetype &type);
// add an entity id to a storage
usize add_entity_id_to_storage(ArchetypeStorage &storage, EntityId entity);
// add a single component to the i_component storage
void add_component_to_storage(ArchetypeStorage &storage, usize i_component, void *data, usize len);
// remove an entity (id + components) from the storage, if entity's row is not last it will be swapped with last
void remove_entity_from_storage(ArchetypeStorage &storage, usize entity_row);

// Components
void add_component(World &world, EntityId entity, ComponentId component_id, void *component_data, usize component_size);
void remove_component(World &world, EntityId entity, ComponentId component_id);
void set_component(World &world, EntityId entity, ComponentId component_id, void *component_data, usize component_size);
bool has_component(World &world, EntityId entity, ComponentId component);
void *get_component(World &world, EntityId entity, ComponentId component_id);

template <typename Component> std::optional<usize> get_component_idx(Archetype type)
{
    return get_component_idx(type, family::type<Component>());
}

// returns a pointer to a component from a query, used to simulate a constexpr loop in get_components_tuples
template <typename Component> Component *tuple_element_component(usize &i_query, usize i_row, const std::vector<u32> query_indices, ArchetypeStorage &storage)
{
    const usize i_component = query_indices[i_query];
    auto &component_storage = storage.components[i_component];
    const usize component_byte_idx = i_row * component_storage.component_size;

    i_query += 1; // for next call
    return reinterpret_cast<Component*>(&component_storage.data[component_byte_idx]);
}


} // namespace impl

struct World
{
    World();

    void display_ui(UI::Context &ctx);

    /// --- Entities

    // Create an entity with a list of components
    template <typename... ComponentTypes> EntityId create_entity(ComponentTypes &&...components)
    {
        EntityId new_entity = family::identifier();
        auto archetype      = impl::create_archetype<ComponentTypes...>();

        // find or create a new bucket for this archetype
        auto storage_h = impl::find_or_create_archetype_storage_from_root(archetypes, archetype);
        auto &storage  = *archetypes.archetype_storages.get(storage_h);

        // add the entity to the entity array
        auto row = impl::add_entity_id_to_storage(storage, new_entity);

        // add the component to every component array, fold expression black magic...
        uint component_i = 0;
        (impl::add_component_to_storage(storage, component_i++, &components, sizeof(ComponentTypes)), ...);
        storage.size += 1;

        // put the entity record in the entity index
        entity_index[new_entity] = EntityRecord{.archetype = storage_h, .row = row};

        return new_entity;
    }

    /// --- Components

    // Add a component to an entity, the entity SHOULD NOT already have that component
    template <typename Component> void add_component(EntityId entity, Component component)
    {
        impl::add_component(*this, entity, family::type<Component>(), &component, sizeof(Component));
    }

    // Remove a component from an entity
    template <typename Component> void remove_component(EntityId entity)
    {
        impl::remove_component(*this, entity, family::type<Component>());
    }

    // Set the value of a component or add it to an entity
    template <typename Component> void set_component(EntityId entity, Component component)
    {
        impl::set_component(*this, entity, family::type<Component>(), &component, sizeof(Component));
    }

    template <typename Component> bool has_component(EntityId entity)
    {
        return impl::has_component(*this, entity, family::type<Component>());
    }

    bool is_component(EntityId entity) { return impl::has_component(*this, entity, family::type<InternalComponent>()); }

    // Get a component from an entity, returns nullptr if not found
    template <typename Component> Component *get_component(EntityId entity)
    {
        return reinterpret_cast<Component *>(impl::get_component(*this, entity, family::type<Component>()));
    }

    template <typename... ComponentTypes, typename Lambda> void for_each(Lambda lambda)
    {
        auto query = impl::create_archetype<ComponentTypes...>();

        for (auto &[h, storage] : archetypes.archetype_storages)
        {
            auto [contains, query_indices] = impl::archetype_contains(query, storage->type);
            if (contains)
            {
                // This archetype contains the queries components
                // add them to the tuples

                //TODO: is it better to add them in a different order? probably, but impossible to do with templates?

                for (usize i_row = 0; i_row < storage->size; i_row++)
                {
                    usize i_query = 0;
                    auto components_tuple = std::make_tuple(impl::tuple_element_component<ComponentTypes>(i_query, i_row, query_indices, *storage)...);
                    lambda(components_tuple);
                }

            }
        }
    }

    // Metadata of entites
    EntityIndex entity_index;
    Archetypes archetypes;
};

}; // namespace my_app::ECS
