#include "engine/render_world_system.h"

#include <exo/hash.h>
#include <exo/logger.h>
#include <exo/profile.h>

#include <gameplay/components/camera_component.h>
#include <gameplay/components/mesh_component.h>
#include <gameplay/entity.h>
#include <gameplay/update_stages.h>
#include <reflection/reflection.h>

PrepareRenderWorld::PrepareRenderWorld()
{
	update_stage = UpdateStage::FrameEnd;
	priority     = 1.0f;
}

void PrepareRenderWorld::initialize(const SystemRegistry &) {}

void PrepareRenderWorld::shutdown() {}

void PrepareRenderWorld::update(const UpdateContext &)
{
	EXO_PROFILE_SCOPE;

	// -- Reset the render world
	render_world.drawable_instances.clear();

	// -- Fill the render world with data from the scene
	ASSERT(main_camera != nullptr);
	render_world.main_camera_view         = main_camera->get_view();
	render_world.main_camera_fov          = main_camera->fov;
	render_world.main_camera_view_inverse = main_camera->get_view_inverse();

	for (auto &[p_entity, mesh_component] : entities) {
		render_world.drawable_instances.emplace_back();
		auto &new_drawable = render_world.drawable_instances.back();

		// HACK: update world transform here :D
		mesh_component->set_local_transform(mesh_component->get_local_transform());

		new_drawable.mesh_asset      = mesh_component->mesh_asset;
		new_drawable.world_transform = mesh_component->get_world_transform();
		new_drawable.world_bounds    = mesh_component->get_world_bounds();
	}
}

void PrepareRenderWorld::register_component(const Entity *entity, refl::BasePtr<BaseComponent> component)
{
	if (auto mesh_component = component.as<MeshComponent>()) {
		auto **entity_component = this->entities.at(entity);
		if (entity_component == nullptr) {
			this->entities.insert(entity, std::move(mesh_component));
		} else {
			*entity_component = mesh_component;
		}
	}
	if (auto camera_component = component.as<CameraComponent>()) {
		main_camera = camera_component;
	}
}

void PrepareRenderWorld::unregister_component(const Entity *entity, refl::BasePtr<BaseComponent> component)
{
	if (component.as<MeshComponent>()) {
		entities.remove(entity);
	}
}
