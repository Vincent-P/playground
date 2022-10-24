#include "gameplay/loading_context.h"

#include "gameplay/system.h"
#include "gameplay/system_registry.h"

void LoadingContext::register_entity_update(Entity *entity)
{
	ASSERT(system_registry->entities_to_update.contains(entity) == false);
	system_registry->entities_to_update.insert(entity);
}

void LoadingContext::unregister_entity_update(Entity *entity)
{
	ASSERT(system_registry->entities_to_update.contains(entity) == true);
	system_registry->entities_to_update.remove(entity);
}

void LoadingContext::register_global_system(Entity *entity, refl::BasePtr<BaseComponent> component)
{
	for (auto system : system_registry->global_systems) {
		system->register_component(entity, component);
	}
}

void LoadingContext::unregister_global_system(Entity *entity, refl::BasePtr<BaseComponent> component)
{
	for (auto system : system_registry->global_systems) {
		system->unregister_component(entity, component);
	}
}
