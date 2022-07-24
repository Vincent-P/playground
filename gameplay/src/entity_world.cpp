#include "gameplay/entity_world.h"

#include "gameplay/component.h"
#include "gameplay/entity.h"
#include "gameplay/loading_context.h"
#include "gameplay/system.h"
#include "gameplay/update_context.h"
#include "gameplay/update_stages.h"

#include <exo/logger.h>

EntityWorld::EntityWorld() { this->str_repo = exo::StringRepository::create(); }

void EntityWorld::update(double delta_t)
{
	LoadingContext loading_context = {&system_registry};

	// -- Prepare entities
	for (auto *entity : entities) {
		if (entity->is_unloaded()) {
			entity->load(loading_context);
		}
		if (entity->is_loaded()) {
			// all components should be initialized
			entity->activate(loading_context);
		}
	}

	// -- Prepare global systems

	for (auto &update_list : global_per_stage_update_list) {
		update_list.clear();
	}

	for (auto *global_system : system_registry.global_systems) {
		global_per_stage_update_list[global_system->update_stage].push_back(global_system);
	}

	for (auto &update_list : global_per_stage_update_list) {
		std::sort(update_list.begin(), update_list.end(), [](GlobalSystem *a, GlobalSystem *b) {
			return a->priority_per_stage[a->update_stage] > b->priority_per_stage[b->update_stage];
		});
	}

	// --

	UpdateContext update_context = {};
	update_context.delta_t       = delta_t;
	for (usize i_stage = 0; i_stage < static_cast<usize>(UpdateStages::Count); i_stage += 1) {
		update_context.stage = static_cast<UpdateStages>(i_stage);

		// TODO: parallel for
		for (auto *entity : entities) {
			if (entity->is_active()) {
				entity->update_systems(update_context);
			}
		}

		for (auto *system : global_per_stage_update_list[update_context.stage]) {
			system->update(update_context);
		}
	}
}

Entity *EntityWorld::create_entity(std::string_view name)
{
	Entity *new_entity = new Entity();
	new_entity->name   = str_repo.intern(name);
	entities.insert(new_entity);
	root_entities.insert(new_entity);
	return new_entity;
}

void EntityWorld::set_parent_entity(Entity *entity, Entity *parent)
{
	entity->parent = parent;
	parent->attached_entities.push_back(entity);
	entity->attach_to_parent();
	parent->refresh_attachments();

	if (root_entities.contains(entity)) {
		root_entities.erase(entity);
	}
}

void EntityWorld::destroy_entity(Entity *entity)
{
	entities.erase(entity);
	if (root_entities.contains(entity)) {
		root_entities.erase(entity);
	}
	delete entity;
}

void EntityWorld::create_system_internal(GlobalSystem *system) { system_registry.global_systems.push_back(system); }

void EntityWorld::destroy_system_internal(GlobalSystem *system)
{
	usize i    = 0;
	usize size = system_registry.global_systems.size();
	for (; i < size; i += 1) {
		if (system_registry.global_systems[i] == system) {
			break;
		}
	}

	if (i < size - 1) {
		std::swap(system_registry.global_systems[i], system_registry.global_systems.back());
	}

	if (i < size) {
		system_registry.global_systems.pop_back();
	}
}
