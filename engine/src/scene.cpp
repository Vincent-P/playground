#include "scene.h"

#include <exo/logger.h>
#include <exo/maths/quaternion.h>
#include <cross/file_dialog.h>

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

#if 0
static void draw_gizmo(float3 camera_world_position, float3 camera_target, float3 camera_up)
{
    constexpr float fov   = 100.f;
    constexpr float size  = 50.f;
    const ImU32     red   = ImGui::GetColorU32(float4(255.f / 256.f, 56.f / 256.f, 86.f / 256.f, 1.0f));
    const ImU32     green = ImGui::GetColorU32(float4(143.f / 256.f, 226.f / 256.f, 10.f / 256.f, 1.0f));
    const ImU32     blue  = ImGui::GetColorU32(float4(52.f / 256.f, 146.f / 256.f, 246.f / 256.f, 1.0f));
    const ImU32     black = ImGui::GetColorU32(float4(0.0f, 0.0f, 0.0f, 1.0f));

    float3 camera_forward  = normalize(camera_target - camera_world_position);
    auto   origin          = float3(0.f);
    float3 camera_position = origin - 2.0f * camera_forward;
    auto   view            = camera::look_at(camera_position, origin, camera_up);
    auto   proj            = camera::perspective(fov, 1.f, 0.01f, 10.0f);

    struct GizmoAxis
    {
        const char *label;
        float3      axis            = {0.0f};
        float2      projected_point = {0.0f};
        ImU32       color;
        bool        draw_line;
    };

    static Vec<GizmoAxis> axes{
        {.label = "X", .axis = float3(1.0f, 0.0f, 0.0f), .color = red, .draw_line = true},
        {.label = "Y", .axis = float3(0.0f, 1.0f, 0.0f), .color = green, .draw_line = true},
        {.label = "Z", .axis = float3(0.0f, 0.0f, 1.0f), .color = blue, .draw_line = true},
        {.label = "-X", .axis = float3(-1.0f, 0.0f, 0.0f), .color = red, .draw_line = false},
        {.label = "-Y", .axis = float3(0.0f, -1.0f, 0.0f), .color = green, .draw_line = false},
        {.label = "-Z", .axis = float3(0.0f, 0.0f, -1.0f), .color = blue, .draw_line = false},
    };

    for (auto &axis : axes)
    {
        // project 3d point to 2d
        float4 projected_p = proj * view * float4(axis.axis, 1.0f);
        projected_p        = (1.0f / projected_p.w) * projected_p;

        // remap [-1, 1] to [-0.9 * size, 0.9 * size] to fit the canvas
        axis.projected_point = 0.9f * size * projected_p.xy();
    }

    // sort by distance to the camera
    auto squared_norm = [](auto v) { return dot(v, v); };
    std::sort(std::begin(axes), std::end(axes), [&](const GizmoAxis &a, const GizmoAxis &b) { return squared_norm(camera_position - a.axis) > squared_norm(camera_position - b.axis); });

    // Set the window position to match the framebuffer right corner
    ImGui::Begin("Framebuffer");
    float2 max     = ImGui::GetWindowContentRegionMax();
    float2 min     = ImGui::GetWindowContentRegionMin();
    float2 fb_size = float2(min.x < max.x ? max.x - min.x : min.x, min.y < max.y ? max.y - min.y : min.y);
    float2 fb_pos  = ImGui::GetWindowPos();
    fb_pos.x += fb_size.x - 2 * size - 10;
    fb_pos.y += 10;
    ImGui::End();

    ImGui::SetNextWindowPos(fb_pos);

    auto flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar;
    ImGui::Begin("Gizmo", nullptr, flags);

    ImDrawList *draw_list = ImGui::GetWindowDrawList();
    float2      p         = ImGui::GetCursorScreenPos();

    // ceenter p
    p = p + float2(size);

    auto font_size = ImGui::GetFontSize();
    auto half_size = float2(font_size / 2.f);
    half_size.x /= 2;

    // draw each axis
    for (const auto &axis : axes)
    {
        if (axis.draw_line)
        {
            draw_list->AddLine(p, p + axis.projected_point, axis.color, 3.0f);
        }

        draw_list->AddCircleFilled(p + axis.projected_point, 7.f, axis.color);

        if (axis.draw_line && axis.label)
        {
            draw_list->AddText(p + axis.projected_point - half_size, black, axis.label);
        }
    }

    ImGui::Dummy(float2(2 * size));

    ImGui::End();
}
#endif

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
    double delta_t       = 0.016;
    entity_world.update(delta_t);
}

void Scene::display_ui(UI::Context &ui)
{
    #if 0
    draw_gizmo(world, main_camera);
    world.display_ui(ui);
    #endif

    auto table_flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInner;
    const auto &assets = asset_manager->get_available_assets();

    if (ui.begin_window("Scene"))
    {
        static cross::UUID selected = {};

        if (ImGui::Button("Import subscene"))
        {
            ImGui::OpenPopup("Choose a subscene");
        }

        if (ImGui::BeginPopupModal("Choose a subscene", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            if (ImGui::BeginTable("AssetMetadataTable", 4, table_flags))
            {
                ImGui::TableSetupColumn("");
                ImGui::TableSetupColumn("UUID");
                ImGui::TableSetupColumn("Name");
                ImGui::TableHeadersRow();

                for (const auto &[uuid, asset_meta] : assets)
                {
                    auto uuid_copy = uuid;
                    ImGui::TableNextRow();

                    ImGui::PushID(&asset_meta);

                    ImGui::TableSetColumnIndex(0);
                    if (ImGui::Button("Choose"))
                    {
                        selected = uuid;
                        ImGui::CloseCurrentPopup();
                    }

                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%.*s", cross::UUID::STR_LEN, uuid.str);

                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("%s", asset_meta.display_name.c_str());

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

        ui.end_window();
    }
}


void Scene::import_mesh(Mesh *mesh)
{
    // import a mesh with identity transform
}

Entity *Scene::import_subscene_rec(const SubScene *subscene, u32 i_node)
{
    const auto &transform  = subscene->transforms[i_node];
    const auto &mesh_asset = subscene->meshes[i_node];
    const auto &children   = subscene->children[i_node];
    const auto &name       = subscene->names[i_node];

    Vec<Entity *> entity_children;
    entity_children.reserve(children.size());
    for (auto i_child : children)
    {
        entity_children.push_back(import_subscene_rec(subscene, i_child));
    }

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
    for (auto *child : entity_children)
    {
        child->set_parent(new_entity);
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
