#pragma once
#include <exo/collections/vector.h>
#include <exo/maths/aabb.h>
#include <exo/maths/matrices.h>
#include <exo/uuid.h>

#include <reflection/reflection.h>

#include <string>

struct LoadingContext;
struct Entity;

enum struct ComponentState
{
	Unloaded,      // Constructed, all properties are set, resources arent loaded yet
	Loading,       //  Resources are still loading
	Loaded,        // All resources are loaded
	LoadingFailed, // One or more resources failed to load
	Initialized    // Allows to allocate (dealocate at shutdown) transient data
};

struct BaseComponent
{
	using Self = BaseComponent;
	REFL_REGISTER_TYPE("BaseComponent")

	exo::UUID   uuid;
	std::string name;

	BaseComponent() = default;
	virtual ~BaseComponent() {}

	virtual void load(LoadingContext &) { state = ComponentState::Loaded; }
	virtual void unload(LoadingContext &) { state = ComponentState::Unloaded; }
	virtual void initialize(LoadingContext &) { state = ComponentState::Initialized; }
	virtual void shutdown(LoadingContext &) { state = ComponentState::Loaded; }
	virtual void update_loading(LoadingContext &) {}

	constexpr bool is_unloaded() const { return state == ComponentState::Unloaded; }
	constexpr bool is_loading() const { return state == ComponentState::Loading; }
	constexpr bool is_loaded() const { return state == ComponentState::Loaded; }
	constexpr bool has_loading_failed() const { return state == ComponentState::LoadingFailed; }
	constexpr bool is_initialized() const { return state == ComponentState::Initialized; }

protected:
	ComponentState state = ComponentState::Unloaded;
};

struct SpatialComponent : BaseComponent
{
	using Self  = SpatialComponent;
	using Super = BaseComponent;
	REFL_REGISTER_TYPE_WITH_SUPER("SpatialComponent")

	void set_local_transform(const float4x4 &new_transform);
	void set_local_bounds(const exo::AABB &new_bounds);

	inline const float4x4  &get_local_transform() const { return local_transform; }
	inline const exo::AABB &get_local_bounds() const { return local_bounds; }
	inline const float4x4  &get_world_transform() const { return world_transform; }
	inline const exo::AABB &get_world_bounds() const { return world_bounds; }

private:
	void update_world_transform();

	float4x4  local_transform = {};
	exo::AABB local_bounds    = {};
	float4x4  world_transform = {};
	exo::AABB world_bounds    = {};

	SpatialComponent       *parent   = nullptr;
	Vec<SpatialComponent *> children = {};

	friend Entity;
};

/**
An entity can have only one spatial component root, so i guess a mesh component is a spatial entity component?
wait at 1h58 there are two skeletal mesh components so they cant be spatial components unless the hierarchy is not in
the slide and the pistol is in fact a child of the human From follow-up stream we can see some comopnents hierarchies:
capsule comp
  - mesh comp (torso)
  - mesh comp (legs)
  - mesh comp (hands)
  - mesh comp (head)
    - mesh comp (glasses)
    - mesh comp (hat)
  - mesh comp (feet)
  - mesh comp (arm)
  - mesh comp (backpack)
weapon comp
...


these mesh comp are probably skeletal mesh components?

so spatial components can be: physics colliders, (skeletal) meshes
**/
