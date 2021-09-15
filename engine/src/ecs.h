#pragma once
#include "ui.h"

#include <exo/prelude.h>
#include <exo/option.h>
#include <exo/collections/pool.h>
#include <exo/collections/vector.h>

#include <unordered_map>
#include <unordered_set>
#include <type_traits>
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

namespace ECS
{

template<typename T>
concept UIable = requires(T o) {
    { o.display_ui() } -> std::same_as<void>;
};
template<typename T>
concept Nameable = requires(T o) {
    { T::type_name() } -> std::same_as<const char*>;
};

template<typename T>
concept TriviallyCopyable = std::is_trivially_copyable<T>::value;

template<typename Component>
concept Componentable = UIable<Component> && Nameable<Component> && TriviallyCopyable<Component>;

// from EnTT, generates a unique unsigned integer per type
struct family
{
    static u64 identifier() noexcept;

    template <Componentable T> static u64 type() noexcept
    {
        static const u64 value = identifier();
        return value;
    }
};

struct EntityId
{
    static EntityId create()
    {
        #if 0
        return {.is_component = 0, .id = family::identifier()};
        #else
        EntityId new_id;
        new_id.is_component = 0;
        new_id.id = family::identifier();
        return new_id;
        #endif
    }

    template<Componentable T> static EntityId component()
    {
        #if 0
        return {.is_component = 1, .id = family::type<T>()};
        #else
        EntityId new_id;
        new_id.is_component = 1;
        new_id.id = family::type<T>();
        return new_id;
        #endif
    }

    bool operator==(EntityId other) const { return raw == other.raw; }

    union
    {
        struct
        {
            u64 id : 63;
            u64 is_component : 1;
        };
        u64 raw;
    };
};

std::string to_string(EntityId id);
} // namespace ECS

namespace std
{
template <> struct hash<ECS::EntityId>
{
    std::size_t operator()(ECS::EntityId const &id) const noexcept { return std::hash<u64>{}(id.raw); }
};
} // namespace std

namespace ECS
{

using ComponentId = EntityId;
// An archetype is a collection of components
using Archetype = Vec<ComponentId>;

// A vector of one component
struct ComponentStorage
{
    // chunks like Unity DOTS?
    Vec<u8> data;   // buffer
    usize component_size{0}; // element size in bytes

    bool operator==(const ComponentStorage&) const = default;
};

struct ArchetypeStorage;
using ArchetypeH = Handle<ArchetypeStorage>;

// Each archetype is stored separately, and contains a SoA of components
struct ArchetypeStorage
{
    // A vector of component's type
    Archetype type;

    // List of entities whose components are stored in this archetype
    Vec<EntityId> entity_ids;

    // List of components indexed by component type
    // (e.g components[family::type<MyComponent>()] contains a list of MyComponent)
    Vec<ComponentStorage> components;
    usize size;

    // Edges to archetype if we add/remove the indexed component type
    // (e.g edges[family::type<MyComponent>()] contains a handle to "this archetype + MyComponent" and "this archetype -
    // MyComponent")
    struct Edge
    {
        ArchetypeH add;
        ArchetypeH remove;
        bool operator==(const Edge&) const = default;
    };
    Vec<Edge> edges;

    bool operator==(const ArchetypeStorage&) const = default;
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

    static const char *type_name() { return "InternalComponent"; }
    void display_ui() {}
};

struct InternalId
{
    const char *tag;

    static const char *type_name() { return "InternalId"; }
    void display_ui() {}
};

struct World;

namespace impl
{
// Archetype
template <Componentable... ComponentTypes> Archetype create_archetype()
{
    constexpr usize component_count = sizeof...(ComponentTypes);
    Archetype result;
    result.resize(component_count);
    usize i = 0;
    ((result[i++] = ComponentId::component<ComponentTypes>()), ...);
    return result;
}

inline bool archetype_contains(Vec<u32> &found, const Archetype &query, const Archetype &archetype)
{
    found.resize(query.size(), u32_invalid);

    if (query.size() > archetype.size())
    {
        return false;
    }

    for (u32 i_archetype = 0; i_archetype < archetype.size(); i_archetype++)
    {
        auto component_id = archetype[i_archetype];
        for (u32 i_query = 0; i_query < query.size(); i_query++)
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
            return false;
        }
    }

    return true;
}

Option<usize> get_component_idx(const Archetype &type, ComponentId component_id);

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

template <Componentable Component> Option<usize> get_component_idx(const Archetype &type)
{
    return get_component_idx(type, family::type<Component>());
}

// returns a reference to a component from a query, used to simulate a constexpr loop in for_each
template <Componentable Component> Component &component_ref(const Archetype &query, usize i_row, const Vec<u32> &query_indices, ArchetypeStorage &storage)
{
    const auto component_id = EntityId::component<Component>();
    usize i_query = 0;
    for (; query[i_query] != component_id; i_query++) {}
    ASSERT(i_query < query.size());

    usize i_component = query_indices[i_query];
    auto &component_storage = storage.components[i_component];
    const usize component_byte_idx = i_row * component_storage.component_size;

    return *reinterpret_cast<Component*>(&component_storage.data[component_byte_idx]);
}

template <Componentable Component> const Component &component_const_ref(const Archetype &query, usize i_row, const Vec<u32> &query_indices, const ArchetypeStorage &storage)
{
    const auto component_id = EntityId::component<Component>();
    usize i_query = 0;
    for (; query[i_query] != component_id; i_query++) {}
    ASSERT(i_query < query.size());

    usize i_component = query_indices[i_query];
    const auto &component_storage = storage.components[i_component];
    const usize component_byte_idx = i_row * component_storage.component_size;

    return *reinterpret_cast<const Component*>(&component_storage.data[component_byte_idx]);
}

} // namespace impl

struct World
{
    World();

    void display_ui(UI::Context &ctx);

    /// --- Entities

    template <Componentable... ComponentTypes>
    EntityId create_entity_internal(EntityId new_entity, ComponentTypes &&...components)
    {
        auto archetype = impl::create_archetype<ComponentTypes...>();

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

    template <Componentable Component> void create_component_if_needed_internal()
    {
        auto component_id = EntityId::component<Component>();
        if (!entity_index.contains(component_id))
        {
            auto [it, inserted] = string_interner.insert(std::string{Component::type_name()});
            create_entity_internal(component_id, InternalComponent{sizeof(Component)}, InternalId{it->c_str()});
        }
    }

    // Create an entity with a list of components
    template <Componentable... ComponentTypes> EntityId create_entity(ComponentTypes &&...components)
    {
        (create_component_if_needed_internal<ComponentTypes>(), ...);

        return create_entity_internal(EntityId::create(), std::forward<ComponentTypes>(components)...);
    }

    // Create an entity with a name and a list of components
    template <Componentable... ComponentTypes> EntityId create_entity(std::string_view name, ComponentTypes &&...components)
    {
        auto [it, inserted] = string_interner.insert(std::string{name});
        return create_entity<InternalId, ComponentTypes...>(InternalId{it->c_str()}, std::forward<ComponentTypes>(components)...);
    }


    /// --- Components

    // Add a component to an entity, the entity SHOULD NOT already have that component
    template <Componentable Component> void add_component(EntityId entity, Component component)
    {
        create_component_if_needed_internal<Component>();
        impl::add_component(*this, entity, ComponentId::component<Component>(), &component, sizeof(Component));
    }

    // Remove a component from an entity
    template <Componentable Component> void remove_component(EntityId entity)
    {
        impl::remove_component(*this, entity, ComponentId::component<Component>());
    }

    // Set the value of a component or add it to an entity
    template <Componentable Component> void set_component(EntityId entity, Component component)
    {
        impl::set_component(*this, entity, ComponentId::component<Component>(), &component, sizeof(Component));
    }

    template <Componentable Component> bool has_component(EntityId entity)
    {
        return impl::has_component(*this, entity, ComponentId::component<Component>());
    }

    bool is_component(EntityId entity) { return entity.is_component; }

    // Get a component from an entity, returns nullptr if not found
    template <Componentable Component> Component *get_component(EntityId entity)
    {
        return reinterpret_cast<Component *>(impl::get_component(*this, entity, ComponentId::component<Component>()));
    }

    template <Componentable... ComponentTypes, typename Lambda> void for_each(Lambda lambda)
    {
        auto query = impl::create_archetype<ComponentTypes...>();
        Vec<u32> query_indices;

        for (auto &[h, storage] : archetypes.archetype_storages)
        {
            if (impl::archetype_contains(query_indices, query, storage->type))
            {
                for (usize i_row = 0; i_row < storage->size; i_row++)
                {
                    // TODO: there is a loop over query in component_ref that could be removed if it was possible
                    // to loop over ComponentTypes for each query and then pass that as arguments to the lambda
                    lambda(impl::component_ref<ComponentTypes>(query, i_row, query_indices, *storage)...);
                }
            }
            query_indices.clear();
        }
    }

    template <Componentable... ComponentTypes, typename Lambda> void for_each(Lambda lambda) const
    {
        auto query = impl::create_archetype<ComponentTypes...>();
        Vec<u32> query_indices;

        for (const auto &[h, storage] : archetypes.archetype_storages)
        {
            if (impl::archetype_contains(query_indices, query, storage->type))
            {
                for (usize i_row = 0; i_row < storage->size; i_row++)
                {
                    // TODO: there is a loop over query in component_ref that could be removed if it was possible
                    // to loop over ComponentTypes for each query and then pass that as arguments to the lambda
                    lambda(impl::component_const_ref<ComponentTypes>(query, i_row, query_indices, *storage)...);
                }
            }
            query_indices.clear();
        }
    }

    /// --- Duplicate for "singleton" components

    // Add a component to an entity, the entity SHOULD NOT already have that component
    template <Componentable Component> void singleton_add_component(Component component)
    {
        create_component_if_needed_internal<Component>();
        impl::add_component(*this, singleton, ComponentId::component<Component>(), &component, sizeof(Component));
    }

    // Remove a component from an entity
    template <Componentable Component> void singleton_remove_component()
    {
        impl::remove_component(*this, singleton, ComponentId::component<Component>());
    }

    // Set the value of a component or add it to an entity
    template <Componentable Component> void singleton_set_component(Component component)
    {
        impl::set_component(*this, singleton, ComponentId::component<Component>(), &component, sizeof(Component));
    }

    template <Componentable Component> bool singleton_has_component()
    {
        return impl::has_component(*this, singleton, ComponentId::component<Component>());
    }

    // Get a component from an entity, returns nullptr if not found
    template <Componentable Component> Component *singleton_get_component()
    {
        return reinterpret_cast<Component *>(impl::get_component(*this, singleton, ComponentId::component<Component>()));
    }

    // Metadata of entites
    EntityIndex entity_index;
    Archetypes archetypes;
    EntityId singleton;
    std::unordered_set<std::string> string_interner;
};

}; // namespace ECS
