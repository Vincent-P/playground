#pragma once
#include <exo/collections/map.h>

#include "engine/render_world.h"
#include <gameplay/system.h>
#include <reflection/reflection.h>

struct RenderWorld;
struct MeshComponent;
struct CameraComponent;

struct PrepareRenderWorld : GlobalSystem
{
	using Self  = PrepareRenderWorld;
	using Super = GlobalSystem;
	REFL_REGISTER_TYPE_WITH_SUPER("PrepareRenderWorld")

	PrepareRenderWorld();

	void initialize(const SystemRegistry &) final;
	void shutdown() final;

	void update(const UpdateContext &) final;

	void register_component(const Entity *entity, refl::BasePtr<BaseComponent> component) final;
	void unregister_component(const Entity *entity, refl::BasePtr<BaseComponent> component) final;

	RenderWorld render_world;

private:
	CameraComponent                          *main_camera;
	exo::Map<const Entity *, MeshComponent *> entities;
};
