#include "engine/scene.h"

#include <exo/logger.h>
#include <exo/maths/quaternion.h>

#include <gameplay/component.h>
#include <gameplay/components/camera_component.h>
#include <gameplay/components/mesh_component.h>
#include <gameplay/entity.h>
#include <gameplay/inputs.h>
#include <gameplay/systems/editor_camera_systems.h>

#include <assets/asset_manager.h>
#include <assets/mesh.h>
#include <assets/subscene.h>

#include "engine/render_world_system.h"

void Scene::init(AssetManager *_asset_manager, const Inputs *inputs)
{
	asset_manager = _asset_manager;

	entity_world.create_system<PrepareRenderWorld>();

	Entity *camera_entity = entity_world.create_entity("Main Camera");
	camera_entity->create_component<CameraComponent>();
	camera_entity->create_component<EditorCameraComponent>();
	camera_entity->create_component<CameraInputComponent>();
	camera_entity->create_system<EditorCameraInputSystem>(inputs);
	camera_entity->create_system<EditorCameraTransformSystem>();

	this->main_camera_entity = camera_entity;
}

void Scene::destroy() {}

void Scene::update(const Inputs &)
{
	double delta_t = 0.016;
	entity_world.update(delta_t);
}

void Scene::import_mesh(Mesh * /*mesh*/)
{
	// import a mesh with identity transform
}

Entity *Scene::import_subscene_rec(const SubScene *subscene, u32 i_node)
{
	const auto &transform  = subscene->transforms[i_node];
	const auto &mesh_asset = subscene->meshes[i_node];
	const auto &children   = subscene->children[i_node];
	const auto &name       = subscene->names[i_node];

	Entity *new_entity = entity_world.create_entity(name);

	SpatialComponent *entity_root = nullptr;
	if (mesh_asset.is_valid()) {
		new_entity->create_component<MeshComponent>();
		auto *mesh_component       = new_entity->get_first_component<MeshComponent>();
		mesh_component->mesh_asset = mesh_asset;
		entity_root                = static_cast<SpatialComponent *>(mesh_component);
	} else {
		new_entity->create_component<SpatialComponent>();
		entity_root = new_entity->get_first_component<SpatialComponent>();
	}

	entity_root->set_local_transform(transform);
	for (auto i_child : children) {
		auto *child = import_subscene_rec(subscene, i_child);
		entity_world.set_parent_entity(child, new_entity);
	}

	return new_entity;
}

void Scene::import_subscene(SubScene *subscene)
{
	for (auto i_root : subscene->roots) {
		import_subscene_rec(subscene, i_root);
	}
}
