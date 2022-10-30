#include "gameplay/entity_world.h"

#include "gameplay/component.h"
#include "gameplay/contexts.h"
#include "gameplay/entity.h"
#include "gameplay/system.h"
#include "gameplay/update_context.h"
#include "gameplay/update_stages.h"

#include <assets/asset_manager.h>
#include <exo/collections/vector.h>
#include <exo/logger.h>
#include <exo/profile.h>
#include <exo/serialization/serializer.h>
#include <exo/uuid.h>

#include <algorithm> // for std::sort

EntityWorld::EntityWorld() { this->str_repo = exo::StringRepository::create(); }

void EntityWorld::update(double delta_t, AssetManager *asset_manager)
{
	EXO_PROFILE_SCOPE;

	LoadingContext        loading_context        = {asset_manager};
	InitializationContext initialization_context = {.system_registry = &this->system_registry};

	// -- Prepare entities
	{
		EXO_PROFILE_SCOPE_NAMED("Prepare entities");
		for (auto &[uuid, entity] : entities) {
			if (entity->is_unloaded()) {
				entity->load(loading_context);
			}
			if (entity->is_loading()) {
				entity->update_loading(loading_context);
			}
			if (entity->is_loaded()) {
				entity->initialize(initialization_context);
			}
		}
	}

	// -- Prepare global systems
	{
		EXO_PROFILE_SCOPE_NAMED("Prepare global systems");
		for (auto &update_list : global_per_stage_update_list) {
			update_list.clear();
		}

		for (auto global_system : system_registry.global_systems) {
			global_per_stage_update_list[global_system->update_stage].push_back(global_system);
		}

		for (auto &update_list : global_per_stage_update_list) {
			std::sort(update_list.begin(), update_list.end(), [](auto a, auto b) { return a->priority > b->priority; });
		}
	}

	// --

	UpdateContext update_context = {};
	update_context.delta_t       = delta_t;
	for (usize i_stage = 0; i_stage < static_cast<usize>(UpdateStage::Count); i_stage += 1) {
		update_context.stage = static_cast<UpdateStage>(i_stage);
		EXO_PROFILE_SCOPE_NAMED("Update stage");

		{
			EXO_PROFILE_SCOPE_NAMED("Entities");
			// TODO: parallel for
			for (auto &[uuid, entity] : entities) {
				if (entity->is_active()) {
					entity->update_systems(update_context);
				}
			}
		}

		{
			EXO_PROFILE_SCOPE_NAMED("Global");
			for (auto system : global_per_stage_update_list[update_context.stage]) {
				system->update(update_context);
			}
		}
	}
}

// -- Entities

Entity *EntityWorld::create_entity(std::string_view name)
{
	auto *new_entity = new Entity();
	new_entity->name = this->str_repo.intern(name);
	new_entity->uuid = exo::UUID::create();

	this->entities.insert(new_entity->uuid, new_entity);
	this->root_entities.insert(new_entity);
	return new_entity;
}

void EntityWorld::set_parent_entity(Entity *entity, Entity *parent)
{
	entity->parent = parent->uuid;
	parent->attached_entities.push_back(entity->uuid);

	this->_attach_to_parent(entity);
	this->_refresh_attachments(parent);

	if (this->root_entities.contains(entity)) {
		this->root_entities.remove(entity);
	}
}

void EntityWorld::destroy_entity(Entity *entity)
{
	entities.remove(entity->uuid);
	if (this->root_entities.contains(entity)) {
		this->root_entities.remove(entity);
	}
	delete entity;
}

void EntityWorld::_attach_to_parent(Entity *entity)
{
	ASSERT(entity->is_attached_to_parent == false);
	ASSERT(entity->parent.is_valid() && (*this->entities.at(entity->parent))->root_component.get() != nullptr);

	Entity *parent      = *this->entities.at(entity->parent);
	auto    parent_root = parent->root_component;

	entity->root_component->parent = parent_root;
	entity->root_component->update_world_transform();
	parent_root->children.push_back(entity->root_component);

	entity->is_attached_to_parent = true;
}

void EntityWorld::_dettach_to_parent(Entity *entity)
{
	ASSERT(entity->is_attached_to_parent == true);
	ASSERT(entity->parent.is_valid() && (*this->entities.at(entity->parent))->root_component.get() != nullptr);

	Entity *parent      = *this->entities.at(entity->parent);
	auto    parent_root = parent->root_component;

	entity->root_component->parent = {};
	entity->root_component->update_world_transform();

	u32 i_parent_child = 0;
	for (; i_parent_child < parent_root->children.size(); i_parent_child += 1) {
		if (parent_root->children[i_parent_child] == entity->root_component) {
			break;
		}
	}

	// Assert hit: The parent doesn't contain this entity in its children
	ASSERT(i_parent_child < parent_root->children.size());

	exo::swap_remove(parent_root->children, i_parent_child);

	entity->is_attached_to_parent = false;
}

void EntityWorld::_refresh_attachments(Entity *entity)
{
	for (auto attached_entity_id : entity->attached_entities) {
		Entity *attached_entity = *this->entities.at(attached_entity_id);
		if (attached_entity->is_attached_to_parent) {
			this->_dettach_to_parent(attached_entity);
			this->_attach_to_parent(attached_entity);
		}
	}
}

// -- Global systems

void EntityWorld::create_system_internal(refl::BasePtr<GlobalSystem> system)
{
	system_registry.global_systems.push_back(system);
}

void EntityWorld::destroy_system_internal(refl::BasePtr<GlobalSystem> system)
{
	usize       i    = 0;
	const usize size = system_registry.global_systems.size();
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

// -- Serializer

void serialize(exo::Serializer &serializer, EntityWorld &world)
{
	if (serializer.is_writing) {
		usize entities_length = world.entities.size;
		exo::serialize(serializer, entities_length);

		for (auto &[uuid, entity] : world.entities) {
			serialize(serializer, *entity);
		}
	} else {
		ASSERT(world.entities.size == 0);
		usize entities_length = 0;
		exo::serialize(serializer, entities_length);

		for (usize i = 0; i < entities_length; ++i) {
			auto *new_entity = new Entity;
			serialize(serializer, *new_entity);
			world.entities.insert(new_entity->uuid, new_entity);

			if (!new_entity->parent.is_valid()) {
				world.root_entities.insert(new_entity);
			}
		}
	}
}
