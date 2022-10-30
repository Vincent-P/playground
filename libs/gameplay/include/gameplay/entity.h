#pragma once
#include <exo/collections/enum_array.h>
#include <exo/collections/vector.h>

#include <exo/uuid.h>

#include "gameplay/update_stages.h"

#include <concepts>
#include <reflection/reflection.h>

namespace exo
{
struct Serializer;
}
struct BaseComponent;
struct SpatialComponent;
struct Entity;
struct UpdateContext;
struct LocalSystem;
struct LoadingContext;
struct InitializationContext;

enum struct EntityState
{
	Unloaded, // all components are unloaded
	Loading,
	Loaded,     // all components are loaded, possible that some are loading (dynamic add)
	Initialized // entity is turned on in the world, components have been registred with all systems
};
inline constexpr const char *to_string(EntityState state)
{
	switch (state) {
	default:
		ASSERT(false);
		[[fallthrough]];
	case EntityState::Unloaded:
		return "Unloaded";
	case EntityState::Loading:
		return "Loading";
	case EntityState::Loaded:
		return "Loaded";
	case EntityState::Initialized:
		return "Initialized";
	}
}

struct Entity
{
	exo::UUID   uuid  = {};
	const char *name  = {};
	EntityState state = EntityState::Unloaded;

	Vec<refl::BasePtr<LocalSystem>>                 local_systems         = {};
	Vec<refl::BasePtr<BaseComponent>>               components            = {};
	exo::EnumArray<Vec<LocalSystem *>, UpdateStage> per_stage_update_list = {};

	refl::BasePtr<SpatialComponent> root_component        = {};
	Vec<exo::UUID>                  attached_entities     = {};
	exo::UUID                       parent                = {};
	bool                            is_attached_to_parent = false;

	// --
	void load(LoadingContext &ctx);
	void unload(LoadingContext &ctx);
	void update_loading(LoadingContext &ctx);
	// Called when an entity finished loading successfully
	void initialize(InitializationContext &ctx);
	// Called just before an entity fully unloads
	void shutdown(InitializationContext &ctx);

	// Call update() on all systems
	void update_systems(const UpdateContext &ctx);

	// Create a local entity system
	template <std::derived_from<LocalSystem> System, typename... Args>
	void create_system(Args &&...args)
	{
		System *new_system = new System(std::forward<Args>(args)...);
		local_systems.push_back(refl::BasePtr<LocalSystem>(new_system));
	}

	// Add a component to the entity
	template <std::derived_from<BaseComponent> Component, typename... Args>
	refl::BasePtr<BaseComponent> create_component(Args &&...args)
	{
		Component *new_component     = new Component(std::forward<Args>(args)...);
		auto       new_component_ptr = refl::BasePtr<BaseComponent>(new_component);
		create_component_internal(new_component_ptr);

		// If the component is the first spatial component, it's the entity's root
		if (auto *spatial_component = refl::upcast<SpatialComponent>(new_component)) {
			if (root_component.get() == nullptr) {
				root_component = refl::BasePtr<SpatialComponent>(spatial_component, refl::typeinfo<Component>());
			}
		}

		return new_component_ptr;
	}

	bool is_active() const { return state == EntityState::Initialized; }
	bool is_loaded() const { return state == EntityState::Loaded; }
	bool is_unloaded() const { return state == EntityState::Unloaded; }
	bool is_loading() const { return state == EntityState::Loading; }

	template <std::derived_from<BaseComponent> Component>
	Component *get_first_component()
	{
		for (auto component : components) {
			auto *derived_component = component.as<Component>();
			if (derived_component != nullptr) {
				return derived_component;
			}
		}
		return nullptr;
	}

	void destroy_system(refl::BasePtr<LocalSystem> system);

	void create_component_internal(refl::BasePtr<BaseComponent> component);
	void destroy_component_internal(refl::BasePtr<BaseComponent> component);
};

void serialize(exo::Serializer &serializer, Entity &entity);
