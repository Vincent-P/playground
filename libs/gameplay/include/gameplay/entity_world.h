#pragma once
#include <exo/collections/set.h>
#include <exo/memory/string_repository.h>
#include <exo/uuid.h>

#include "gameplay/system.h"
#include "gameplay/system_registry.h"

#include <string_view>

namespace exo
{
struct Serializer;
}
struct Entity;

struct EntityWorld
{
	exo::StringRepository         str_repo        = {};
	exo::Map<exo::UUID, Entity *> entities        = {};
	exo::Set<Entity *>            root_entities   = {};
	SystemRegistry                system_registry = {};

	exo::EnumArray<Vec<refl::BasePtr<GlobalSystem>>, UpdateStage> global_per_stage_update_list = {};

	// --
	EntityWorld();
	void update(double delta_t);

	// Entities
	Entity *create_entity(std::string_view name = "Unnamed");
	void    destroy_entity(Entity *entity);
	void    set_parent_entity(Entity *entity, Entity *parent);

	void _attach_to_parent(Entity *entity);
	void _dettach_to_parent(Entity *entity);
	void _refresh_attachments(Entity *entity);

	// Global systems
	template <std::derived_from<GlobalSystem> System, typename... Args>
	void create_system(Args &&...args)
	{
		System *new_system = new System(std::forward<Args>(args)...);
		create_system_internal(refl::BasePtr<GlobalSystem>{new_system});
	}
	void create_system_internal(refl::BasePtr<GlobalSystem> system);
	void destroy_system_internal(refl::BasePtr<GlobalSystem> system);

	const SystemRegistry &get_system_registry() const { return system_registry; }
	SystemRegistry       &get_system_registry() { return system_registry; }
};

void serialize(exo::Serializer &serializer, EntityWorld &world);
