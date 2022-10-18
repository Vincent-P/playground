#include "engine/scene.h"

#include <exo/hash.h>
#include <exo/logger.h>
#include <exo/maths/numerics.h>
#include <exo/maths/quaternion.h>

#include <assets/asset_manager.h>
#include <assets/mesh.h>
#include <assets/subscene.h>
#include <engine/render_world_system.h>
#include <gameplay/component.h>
#include <gameplay/components/camera_component.h>
#include <gameplay/components/mesh_component.h>
#include <gameplay/entity.h>
#include <gameplay/inputs.h>
#include <gameplay/systems/editor_camera_systems.h>
#include <painter/painter.h>
#include <ui/ui.h>

#include <fmt/format.h>

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

static void tree_view_entity(
	ui::Ui &ui, SceneUi &scene_ui, Rect &content_rect, Entity *entity, float indentation = 1.0f)
{
	if (content_rect.size.y < ui.theme.font_size) {
		return;
	}

	auto content_rectsplit = RectSplit{content_rect, SplitDirection::Top};
	auto line_rect         = content_rectsplit.split(2.0f * ui.theme.font_size);
	auto line_rectsplit    = RectSplit{line_rect, SplitDirection::Left};

	auto *entity_scene_ui = scene_ui.entity_uis.at(entity);
	if (!entity_scene_ui) {
		entity_scene_ui = scene_ui.entity_uis.insert(entity, {});
	}

	auto       &entity_opened = entity_scene_ui->treeview_opened;
	const char *label         = entity_opened ? "_" : ">";
	if (!entity->attached_entities.empty() && ui::button_split(ui, line_rectsplit, label)) {
		entity_opened = !entity_opened;
	}
	auto margin_rect = line_rectsplit.split(indentation * 1.0f * ui.theme.font_size);
	if (scene_ui.selected_entity == entity) {
		painter_draw_color_rect(*ui.painter, margin_rect, u32_invalid, ColorU32::from_floats(0.7f, 0.4f, 0.1f));
	}

	auto entity_label = fmt::format("Name: {}", entity->name);
	auto label_rect   = ui::label_split(ui, line_rectsplit, entity_label);

	if (ui::invisible_button(ui, label_rect)) {
		if (scene_ui.selected_entity == entity) {
			scene_ui.selected_entity = nullptr;
		} else {
			scene_ui.selected_entity = entity;
		}
	}

	if (entity_opened) {
		indentation += 1.0f;
		for (auto *child : entity->attached_entities) {
			tree_view_entity(ui, scene_ui, content_rect, child, indentation);
		}
	}
}

void scene_treeview_ui(ui::Ui &ui, Scene &scene, Rect &content_rect)
{
	auto &world = scene.entity_world;

	auto rectsplit = RectSplit{content_rect, SplitDirection::Top};
	ui::label_split(ui, rectsplit, fmt::format("Entities: {}", world.entities.size));
	/*auto margin_rect =*/rectsplit.split(1.0f * ui.theme.font_size);

	for (auto *entity : world.root_entities) {
		tree_view_entity(ui, scene.ui, content_rect, entity);
	}
}

void scene_inspector_ui(ui::Ui &ui, Scene &scene, Rect &content_rect)
{
	if (scene.ui.selected_entity == nullptr) {
		return;
	}

	auto *entity = scene.ui.selected_entity;

	auto content_rectsplit = RectSplit{content_rect, SplitDirection::Top};

	auto entity_label = fmt::format("Selected: {}", entity->name);
	ui::label_split(ui, content_rectsplit, entity_label);
}

void Scene::update(const Inputs &)
{
	const double delta_t = 0.016;
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
