#pragma once
#include <exo/collections/map.h>
#include <exo/collections/pool.h>

#include <gameplay/entity_world.h>

class Inputs;
struct AssetManager;
struct Mesh;
struct SubScene;
struct EntityWorld;
struct Rect;
namespace ui
{
struct Ui;
}

struct EntitySceneUi
{
	bool treeview_opened = false;
};
struct SceneUi
{
	Entity                           *selected_entity = nullptr;
	exo::Map<Entity *, EntitySceneUi> entity_uis      = {};
};

struct Scene
{
	void init(AssetManager *_asset_manager, const Inputs *inputs);
	void destroy();
	void update(const Inputs &inputs);

	void    import_mesh(Mesh *mesh);
	Entity *import_subscene_rec(const SubScene *subscene, u32 i_node);
	void    import_subscene(SubScene *subscene);

	AssetManager *asset_manager;
	EntityWorld   entity_world;
	Entity       *main_camera_entity = nullptr;

	SceneUi ui = {};
};

void scene_treeview_ui(ui::Ui &ui, Scene &scene, Rect &content_rect);
void scene_inspector_ui(ui::Ui &ui, Scene &scene, Rect &content_rect);
