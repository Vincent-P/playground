#include "scene.h"

#include <exo/base/logger.h>
#include <exo/maths/quaternion.h>
#include <exo/cross/file_dialog.h>

#include "gameplay/component.h"
#include "inputs.h"
#include "ui.h"

#include "assets/asset_manager.h"
#include "assets/subscene.h"
#include "assets/mesh.h"

#include "gameplay/entity.h"
#include "gameplay/components/camera_component.h"
#include "gameplay/components/mesh_component.h"

#include "gameplay/systems/editor_camera_systems.h"
#include "render/render_world_system.h"

#include <imgui/imgui.h>
#include <leaf.hpp>


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

void Scene::destroy()
{
}

void Scene::update(const Inputs &)
{
    ZoneScoped;
    double delta_t       = 0.016;
    entity_world.update(delta_t);
}

void Scene::display_ui()
{
    ZoneScoped;
    #if 0
    draw_gizmo(world, main_camera);
    world.display_ui();
    #endif

    auto table_flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInner;
    const auto &assets = asset_manager->get_assets();

    static Vec<cross::UUID> sorted_assets;
    sorted_assets.clear();
    for (const auto &[uuid, asset_meta] : assets)
    {
        sorted_assets.push_back(uuid);
    }

    std::sort(sorted_assets.begin(), sorted_assets.end(), [&](auto lhs, auto rhs) {
        return std::strncmp(assets.at(lhs)->uuid.str, assets.at(rhs)->uuid.str, cross::UUID::STR_LEN) > 0;
    });

    if (auto w = UI::begin_window("Scene"))
    {
        static cross::UUID selected = {};

        if (ImGui::Button("Import subscene"))
        {
            ImGui::OpenPopup("Choose a subscene");
        }

        if (ImGui::BeginPopupModal("Choose a subscene", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            if (ImGui::Button("Cancel"))
            {
                ImGui::CloseCurrentPopup();
            }

            if (ImGui::BeginTable("AssetMetadataTable", 4, table_flags))
            {
                ImGui::TableSetupColumn("");
                ImGui::TableSetupColumn("Type");
                ImGui::TableSetupColumn("UUID");
                ImGui::TableSetupColumn("Name");
                ImGui::TableHeadersRow();

                for (auto &uuid : sorted_assets)
                {
                    const auto *asset = assets.at(uuid);
                    if (dynamic_cast<const SubScene*>(asset) == nullptr)
                    {
                        continue;
                    }

                    ImGui::TableNextRow();

                    ImGui::PushID(asset);

                    ImGui::TableSetColumnIndex(0);
                    if (ImGui::Button("Choose"))
                    {
                        selected = uuid;
                        ImGui::CloseCurrentPopup();
                    }

                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%s", asset->type_name());

                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("%.*s", static_cast<int>(cross::UUID::STR_LEN), uuid.str);

                    ImGui::TableSetColumnIndex(3);
                    ImGui::Text("%s", asset->name);

                    ImGui::PopID();
                }

                ImGui::EndTable();
            }
            ImGui::EndPopup();
        }

        if (selected.is_valid())
        {
            logger::info("Selected {}\n", selected);

            leaf::try_handle_all(
                [&]() -> Result<void>
                {
                    BOOST_LEAF_AUTO(selected_asset, asset_manager->load_asset(selected));
                    if (auto *p_subscene = dynamic_cast<SubScene*>(selected_asset))
                    {
                        this->import_subscene(p_subscene);
                    }
                    else if (auto *p_mesh = dynamic_cast<Mesh*>(selected_asset))
                    {
                        this->import_mesh(p_mesh);
                    }
                    return Ok<void>();
                },
                asset_manager->get_error_handlers());
        }
        selected = {};
    }
}

void Scene::import_mesh(Mesh */*mesh*/)
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
    if (mesh_asset.is_valid())
    {
        new_entity->create_component<MeshComponent>();
        auto *mesh_component       = new_entity->get_first_component<MeshComponent>();
        mesh_component->mesh_asset = mesh_asset;
        entity_root = static_cast<SpatialComponent*>(mesh_component);
    }
    else
    {
        new_entity->create_component<SpatialComponent>();
        entity_root       = new_entity->get_first_component<SpatialComponent>();
    }

    entity_root->set_local_transform(transform);
    for (auto i_child : children)
    {
        auto *child = import_subscene_rec(subscene, i_child);
        entity_world.set_parent_entity(child, new_entity);
    }

    return new_entity;
}

void Scene::import_subscene(SubScene *subscene)
{
    for (auto i_root : subscene->roots)
    {
        auto *new_root = import_subscene_rec(subscene, i_root);
        UNUSED(new_root);
    }
}
