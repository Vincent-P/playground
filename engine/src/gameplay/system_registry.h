#pragma once
#include <exo/collections/vector.h>
#include <exo/collections/set.h>

struct Entity;
struct GlobalSystem;

struct SystemRegistry
{
    Set<Entity*> entities_to_update;
    Vec<GlobalSystem*> global_systems;
};
