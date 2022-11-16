#pragma once
#include "exo/collections/set.h"
#include "exo/collections/vector.h"
#include "reflection/reflection.h"

struct Entity;
struct GlobalSystem;

struct SystemRegistry
{
	template <std::derived_from<GlobalSystem> System>
	const System *get_system() const
	{
		for (const auto global_system : global_systems) {
			if (auto *derived_system = global_system.as<System>()) {
				return derived_system;
			}
		}
		return nullptr;
	}

	template <std::derived_from<GlobalSystem> System>
	System *get_system()
	{
		for (auto global_system : global_systems) {
			if (auto *derived_system = global_system.as<System>()) {
				return derived_system;
			}
		}
		return nullptr;
	}

	exo::Set<Entity *>  entities_to_update;
	Vec<refl::BasePtr<GlobalSystem>> global_systems;
};
