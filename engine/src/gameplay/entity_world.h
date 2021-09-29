#pragma once
#include <exo/collections/set.h>

#include "gameplay/system_registry.h"

struct Entity;

struct EntityWorld
{
public:
    Entity* create_entity();
    void destroy_entity(Entity *entity);
    void update(double delta_t);

private:
    Set<Entity*> entities;
    SystemRegistry system_registry;
};
