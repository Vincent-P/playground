#pragma once
#include <exo/prelude.h>
#include <exo/collections/vector.h>
#include <exo/collections/enum_array.h>

#include <cross/uuid.h>

#include "gameplay/update_stages.h"

#include <concepts>

struct BaseComponent;
struct SpatialComponent;
struct Entity;
struct UpdateContext;
struct LocalSystem;
struct LoadingContext;

enum struct EntityState
{
    Unloaded, // all components are unloaded
    Loaded,   // all components are loaded, possible that some are loading (dynamic add)
    Activated // entity is turned on in the world, components have been registred with all systems
};

struct Entity
{
    void load(LoadingContext &ctx);
    void unload(LoadingContext &ctx);

    // Called when an entity finished loading successfully
    // Registers each component with all local systems
    // Create per-stage local system update lists
    // Registers each component with all global systems
    // Create entity attachment (if required)
    void activate(LoadingContext &ctx);

    // Called just before an entity fully unloads
    void deactivate(LoadingContext &ctx);

    void update_systems(const UpdateContext &ctx);

    template<std::derived_from<LocalSystem> System, typename ...Args>
    void create_system(Args &&...args)
    {
        System *new_system = new System(std::forward<Args>(args)...);
        create_system_internal(reinterpret_cast<LocalSystem*>(new_system));
    }

    template<std::derived_from<BaseComponent> Component, typename ...Args>
    void create_component(Args &&...args)
    {
        Component *new_component = new Component(std::forward<Args>(args)...);
        create_component_internal(reinterpret_cast<BaseComponent*>(new_component));
    }

    bool is_active() const { return state == EntityState::Activated; }
    bool is_loaded() const { return state == EntityState::Loaded; }
    bool is_unloaded() const { return state == EntityState::Unloaded; }

    template<std::derived_from<BaseComponent> Component>
    Component *get_first_component()
    {
        for (auto *component : components)
        {
            auto *derived_component = dynamic_cast<Component*>(component);
            if (derived_component != nullptr)
            {
                return derived_component;
            }
        }
        return nullptr;
    }

private:

    // create_system
    void create_system_internal(LocalSystem *system);
    void destroy_system_internal(LocalSystem *system);

    void create_component_internal(BaseComponent *component);
    void destroy_component_internal(BaseComponent *component);

    cross::UUID uuid;
    std::string name;
    EntityState state = EntityState::Unloaded;

    Vec<LocalSystem *>   local_systems;
    Vec<BaseComponent *> components;
    EnumArray<Vec<LocalSystem *>, UpdateStages> per_stage_update_list;

    SpatialComponent *   root_component = nullptr;
};
