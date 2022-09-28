#pragma once

struct Entity;
struct BaseComponent;
struct SystemRegistry;

struct LoadingContext
{
	void register_entity_update(Entity *entity);
	void unregister_entity_update(Entity *entity);

	void register_global_system(Entity *entity, BaseComponent *component);
	/**
	   for system in global_systems;
	     system.register_component(entity, component);
	 **/
	void unregister_global_system(Entity *entity, BaseComponent *component);
	/**
	   for system in global_systems;
	     system.register_component(entity, component);
	 **/

	SystemRegistry *system_registry;
};
