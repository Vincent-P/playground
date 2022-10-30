#include "engine/scene.h"

#include <assets/asset_id_formatter.h>
#include <assets/asset_manager.h>
#include <assets/mesh.h>
#include <assets/subscene.h>
#include <cross/mapped_file.h>
#include <engine/render_world_system.h>
#include <exo/hash.h>
#include <exo/logger.h>
#include <exo/maths/numerics.h>
#include <exo/maths/quaternion.h>
#include <exo/serialization/serializer_helper.h>
#include <exo/uuid.h>
#include <exo/uuid_formatter.h>
#include <gameplay/component.h>
#include <gameplay/components/camera_component.h>
#include <gameplay/components/mesh_component.h>
#include <gameplay/entity.h>
#include <gameplay/inputs.h>
#include <gameplay/systems/editor_camera_systems.h>
#include <painter/painter.h>
#include <reflection/reflection.h>
#include <ui/ui.h>

#include <fmt/format.h>

void Scene::init(AssetManager *_asset_manager, const Inputs *inputs)
{
	asset_manager = _asset_manager;

	auto last_imported_scene = cross::MappedFile::open(ASSET_PATH "/last_imported_scene.asset");
	if (last_imported_scene) {
		exo::serializer_helper::read_object(last_imported_scene.value().content(), this->entity_world);
	}

	entity_world.create_system<PrepareRenderWorld>();

	Entity *camera_entity = nullptr;
	for (auto *entity : entity_world.root_entities) {
		if (entity->name == std::string_view{"Main Camera"}) {
			camera_entity = entity;
			break;
		}
	}
	if (!camera_entity) {
		camera_entity = entity_world.create_entity("Main Camera");
		camera_entity->create_component<CameraComponent>();
		camera_entity->create_component<EditorCameraComponent>();
		camera_entity->create_component<CameraInputComponent>();
	}
	camera_entity->create_system<EditorCameraInputSystem>(inputs);
	camera_entity->create_system<EditorCameraTransformSystem>();

	this->main_camera_entity = camera_entity;
}

void Scene::destroy() {}

static void tree_view_entity(ui::Ui &ui,
	SceneUi                         &scene_ui,
	Rect                            &content_rect,
	EntityWorld                     &world,
	exo::UUID                        entity_id,
	float                            indentation = 1.0f)
{
	if (content_rect.size.y < ui.theme.font_size) {
		return;
	}

	Entity *entity = *world.entities.at(entity_id);

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
		for (auto child_id : entity->attached_entities) {
			tree_view_entity(ui, scene_ui, content_rect, world, child_id, indentation);
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
		tree_view_entity(ui, scene.ui, content_rect, world, entity->uuid);
	}
}

void ui_matrix_label(ui::Ui &ui, const float4x4 &matrix, RectSplit &rectsplit)
{
	char label_buf[256] = {};

	auto res = fmt::format_to_n(label_buf,
		256,
		"{} {} {} {}",
		matrix.at(0, 0),
		matrix.at(0, 1),
		matrix.at(0, 2),
		matrix.at(0, 3));

	ui::label_split(ui, rectsplit, std::string_view{&label_buf[0], res.size});

	res = fmt::format_to_n(label_buf,
		256,
		"{} {} {} {}",
		matrix.at(1, 0),
		matrix.at(1, 1),
		matrix.at(1, 2),
		matrix.at(1, 3));

	ui::label_split(ui, rectsplit, std::string_view{&label_buf[0], res.size});

	res = fmt::format_to_n(label_buf,
		256,
		"{} {} {} {}",
		matrix.at(2, 0),
		matrix.at(2, 1),
		matrix.at(2, 2),
		matrix.at(2, 3));

	ui::label_split(ui, rectsplit, std::string_view{&label_buf[0], res.size});

	res = fmt::format_to_n(label_buf,
		256,
		"{} {} {} {}",
		matrix.at(3, 0),
		matrix.at(3, 1),
		matrix.at(3, 2),
		matrix.at(3, 3));

	ui::label_split(ui, rectsplit, std::string_view{&label_buf[0], res.size});
}

void scene_inspector_spatial_component(ui::Ui &ui, SpatialComponent *component, Rect &content_rect)
{
	auto line_rectsplit = RectSplit{content_rect, SplitDirection::Top};

	ui::label_split(ui, line_rectsplit, "Local transform:");
	ui_matrix_label(ui, component->get_local_transform(), line_rectsplit);
}

void scene_inspector_component_ui(ui::Ui &ui, refl::BasePtr<BaseComponent> component, Rect &content_rect)
{
	auto line_rectsplit = RectSplit{content_rect, SplitDirection::Top};

	char label_buf[256] = {};

	auto res = fmt::format_to_n(label_buf,
		256,
		"{} [{} ({} bytes)]",
		component->name,
		component.typeinfo().name,
		component.typeinfo().size);

	ui::label_split(ui, line_rectsplit, std::string_view{&label_buf[0], res.size});

	res = fmt::format_to_n(label_buf, 256, "State: {}", to_string(component->state));
	ui::label_split(ui, line_rectsplit, std::string_view{&label_buf[0], res.size});

	res = fmt::format_to_n(label_buf, 256, "UUID: {}", component->uuid);
	ui::label_split(ui, line_rectsplit, std::string_view{&label_buf[0], res.size});

	if (auto *mesh_component = component.as<MeshComponent>()) {
		res = fmt::format_to_n(label_buf, 256, "Mesh: {}", mesh_component->mesh_asset);
		ui::label_split(ui, line_rectsplit, std::string_view{&label_buf[0], res.size});
		scene_inspector_spatial_component(ui, static_cast<SpatialComponent *>(mesh_component), line_rectsplit.rect);
	} else if (auto *spatial_component = component.as<SpatialComponent>()) {
		scene_inspector_spatial_component(ui, spatial_component, line_rectsplit.rect);
	}
}

void scene_inspector_ui(ui::Ui &ui, Scene &scene, Rect &content_rect)
{
	if (scene.ui.selected_entity == nullptr) {
		return;
	}

	char label_buf[256] = {};
	auto em             = ui.theme.font_size;

	auto *entity = scene.ui.selected_entity;

	auto content_rectsplit = RectSplit{content_rect, SplitDirection::Top};

	auto res = fmt::format_to_n(label_buf, 256, "Selected: {}", entity->name);
	ui::label_split(ui, content_rectsplit, std::string_view{&label_buf[0], res.size});

	res = fmt::format_to_n(label_buf, 256, "State: {}", to_string(entity->state));
	ui::label_split(ui, content_rectsplit, std::string_view{&label_buf[0], res.size});

	content_rectsplit.split(1.0f * em);

	ui::label_split(ui, content_rectsplit, "Components:");
	for (const auto component : entity->components) {
		scene_inspector_component_ui(ui, component, content_rect);
	}

	content_rectsplit.split(1.0f * em);

	ui::label_split(ui, content_rectsplit, "Local systems:");
	for (auto system : entity->local_systems) {
		ui::label_split(ui, content_rectsplit, system.typeinfo().name);
	}

	content_rectsplit.split(1.0f * em);
}

void Scene::update(const Inputs &)
{
	const double delta_t = 0.016;
	entity_world.update(delta_t, this->asset_manager);
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
		auto *mesh_component       = new_entity->create_component<MeshComponent>().as<MeshComponent>();
		mesh_component->mesh_asset = mesh_asset;
		entity_root                = static_cast<SpatialComponent *>(mesh_component);
	} else {
		entity_root = new_entity->create_component<SpatialComponent>().as<SpatialComponent>();
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

	exo::serializer_helper::write_object_to_file(ASSET_PATH "/last_imported_scene.asset", this->entity_world);
}
