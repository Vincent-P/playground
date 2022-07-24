#pragma once
#include <exo/collections/set.h>
#include <exo/collections/vector.h>

struct Entity;
struct GlobalSystem;

struct SystemRegistry
{
	template <std::derived_from<GlobalSystem> System> const System *get_system() const
	{
		for (const GlobalSystem *global_system : global_systems) {
			if (auto derived_system = dynamic_cast<const System *>(global_system)) {
				return derived_system;
			}
		}
		return nullptr;
	}

	template <std::derived_from<GlobalSystem> System> System *get_system()
	{
		for (GlobalSystem *global_system : global_systems) {
			if (auto derived_system = dynamic_cast<System *>(global_system)) {
				return derived_system;
			}
		}
		return nullptr;
	}

	exo::Set<Entity *>  entities_to_update;
	Vec<GlobalSystem *> global_systems;
};
