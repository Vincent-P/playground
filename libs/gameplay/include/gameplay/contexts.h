#pragma once
#include "reflection/reflection.h"

struct Entity;
struct BaseComponent;
struct SystemRegistry;
struct AssetManager;

struct InitializationContext
{
	void register_entity_update(Entity *entity);
	void unregister_entity_update(Entity *entity);

	void register_global_system(Entity *entity, refl::BasePtr<BaseComponent> component);
	/**
	   for system in global_systems;
	     system.register_component(entity, component);
	 **/
	void unregister_global_system(Entity *entity, refl::BasePtr<BaseComponent> component);
	/**
	   for system in global_systems;
	     system.register_component(entity, component);
	 **/

	SystemRegistry *system_registry;
};

struct LoadingContext
{
	AssetManager *asset_manager;
};
