#pragma once
#include <exo/collections/enum_array.h>
#include <reflection/reflection.h>

#include "gameplay/update_stages.h"

struct Entity;
struct BaseComponent;

struct SystemRegistry;
struct UpdateContext;

struct LocalSystem
{
	using Self = LocalSystem;
	REFL_REGISTER_TYPE("LocalSystem")

	UpdateStage update_stage = UpdateStage::FrameStart;
	float       priority     = -1.0f;

	// --
	virtual ~LocalSystem() {}
	virtual void update(const UpdateContext &ctx)                             = 0;
	virtual void register_component(refl::BasePtr<BaseComponent> component)   = 0;
	virtual void unregister_component(refl::BasePtr<BaseComponent> component) = 0;
};

struct GlobalSystem
{
	using Self = GlobalSystem;
	REFL_REGISTER_TYPE("GlobalSystem")

	UpdateStage update_stage = UpdateStage::FrameStart;
	float       priority     = -1.0f;

	// --
	friend struct EntityWorld;
	friend struct LoadingContext;

	virtual ~GlobalSystem() {}
	virtual void initialize(const SystemRegistry &) = 0;
	virtual void shutdown()                         = 0;

	virtual void update(const UpdateContext &)                                                      = 0;
	virtual void register_component(const Entity *entity, refl::BasePtr<BaseComponent> component)   = 0;
	virtual void unregister_component(const Entity *entity, refl::BasePtr<BaseComponent> component) = 0;
};
