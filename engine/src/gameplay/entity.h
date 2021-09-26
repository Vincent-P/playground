#pragma once
#include <exo/prelude.h>
#include <exo/collections/vector.h>
#include <exo/collections/enum_array.h>

#include "gameplay/system.h" // for UpdateStages

struct BaseComponent;
struct BaseSystem;
struct SpatialEntityComponent;

enum struct EntityState
{
    Unloaded, // all components are unloaded
    Loaded, // all components are loaded, possible that some are loading (dynamic add)
    Activated // entity is turned on in the world, components have been registred with all systems
};

struct Entity
{
    u64 uid;

    void load();
    void unload();

    // Registers each component with all local systems
    // Create per-stage local system update lists
    // Registers each component with all global systems
    // Create entity attachment (if required)
    void activate();

    // Opposite of activate()
    void deactivate();

    SpatialEntityComponent *root_component = nullptr;
    Vec<BaseComponent *> components;
    Vec<BaseSystem *>    local_systems;

    EnumArray<Vec<BaseSystem*>, UpdateStages> per_stage_update_list;
};
