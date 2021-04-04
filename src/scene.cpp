#include "scene.hpp"

#include "gltf.hpp"
#include "inputs.hpp"
#include "camera.hpp"

#include "base/logger.hpp"

#include "platform/file_dialog.hpp"

#include "components/camera_component.hpp"
#include "components/input_camera_component.hpp"
#include "components/sky_atmosphere_component.hpp"
#include "components/mesh_component.hpp"
#include "components/transform_component.hpp"

#include <fmt/format.h>

static void draw_gizmo(ECS::World &world, ECS::EntityId main_camera)
{
    const auto &camera_transform = *world.get_component<TransformComponent>(main_camera);
    const auto &input_camera = *world.get_component<InputCameraComponent>(main_camera);

    constexpr float fov  = 100.f;
    constexpr float size = 50.f;
    const ImGuiCol red   = ImGui::GetColorU32(float4(255.f / 256.f, 56.f / 256.f, 86.f / 256.f, 1.0f));
    const ImGuiCol green = ImGui::GetColorU32(float4(143.f / 256.f, 226.f / 256.f, 10.f / 256.f, 1.0f));
    const ImGuiCol blue  = ImGui::GetColorU32(float4(52.f / 256.f, 146.f / 256.f, 246.f / 256.f, 1.0f));
    const ImGuiCol black = ImGui::GetColorU32(float4(0.0f, 0.0f, 0.0f, 1.0f));

    float3 camera_forward  = normalize(input_camera.target - camera_transform.position);
    auto origin          = float3(0.f);
    float3 camera_position = origin - 2.0f * camera_forward;
    auto view              = camera::look_at(camera_position, origin, camera_transform.up);
    auto proj              = camera::perspective(fov, 1.f, 0.01f, 10.0f);

    struct GizmoAxis
    {
        const char *label;
        float3 axis;
        float2 projected_point;
        ImGuiCol color;
        bool draw_line;
    };

    Vec<GizmoAxis> axes{
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
    std::sort(std::begin(axes), std::end(axes), [&](const GizmoAxis &a, const GizmoAxis &b) {
        return (camera_position - a.axis).squared_norm() > (camera_position - b.axis).squared_norm();
    });


    // Set the window position to match the framebuffer right corner
    ImGui::Begin("Framebuffer");
    float2 max = ImGui::GetWindowContentRegionMax();
    float2 min = ImGui::GetWindowContentRegionMin();
    float2 fb_size = float2(min.x < max.x ? max.x - min.x : min.x, min.y < max.y ? max.y - min.y : min.y);
    float2 fb_pos = ImGui::GetWindowPos();
    fb_pos.x += fb_size.x - 2 * size - 10;
    fb_pos.y += 10;
    ImGui::End();

    ImGui::SetNextWindowPos(fb_pos);

    auto flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoResize
                 | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar;
    ImGui::Begin("Gizmo", nullptr, flags);

    ImDrawList *draw_list = ImGui::GetWindowDrawList();
    float2 p              = ImGui::GetCursorScreenPos();

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


void Scene::init()
{
    main_camera = world.create_entity(std::string_view{"Camera"}, TransformComponent{}, CameraComponent{}, InputCameraComponent{});
    world.singleton_add_component(SkyAtmosphereComponent{});
}

void Scene::destroy()
{
}

void Scene::update(const Inputs &inputs)
{
    constexpr float CAMERA_MOVE_SPEED   = 5.0f;
    constexpr float CAMERA_ROTATE_SPEED = 80.0f;
    constexpr float CAMERA_SCROLL_SPEED = 80.0f;

    // InputCamera inputs
    world.for_each<TransformComponent, InputCameraComponent>([&](const auto &transform, auto &input_camera) {
        using States = InputCameraComponent::States;

        float delta_t      = 0.016f;
        bool camera_active = inputs.is_pressed(Action::CameraModifier);
        bool camera_move   = inputs.is_pressed(Action::CameraMove);
        bool camera_orbit  = inputs.is_pressed(Action::CameraOrbit);

        // TODO: transition table DSL?
        // state transition
        switch (input_camera.state)
        {
            case States::Idle:
            {
                if (camera_active && camera_move)
                {
                    input_camera.state = States::Move;
                }
                else if (camera_active && camera_orbit)
                {
                    input_camera.state = States::Orbit;
                }
                else if (camera_active)
                {
                    input_camera.state = States::Zoom;
                }
                else
                {

                    // handle inputs
                    if (auto scroll = inputs.get_scroll_this_frame())
                    {
                        input_camera.target.y += (CAMERA_SCROLL_SPEED * delta_t) * scroll->y;
                    }
                }

                break;
            }
            case States::Move:
            {
                if (!camera_active || !camera_move)
                {
                    input_camera.state = States::Idle;
                }
                else
                {

                    // handle inputs
                    if (auto mouse_delta = inputs.get_mouse_delta())
                    {
                        auto up    = float(mouse_delta->y);
                        auto right = float(mouse_delta->x);

                        auto camera_plane_forward = normalize(float3(transform.front.x, 0.0f, transform.front.z));
                        auto camera_right         = cross(transform.up, transform.front);
                        auto camera_plane_right   = normalize(float3(camera_right.x, 0.0f, camera_right.z));

                        input_camera.target
                            = input_camera.target + CAMERA_MOVE_SPEED * delta_t * right * camera_plane_right;
                        input_camera.target
                            = input_camera.target + CAMERA_MOVE_SPEED * delta_t * up * camera_plane_forward;
                    }
                }
                break;
            }
            case States::Orbit:
            {
                if (!camera_active || !camera_orbit)
                {
                    input_camera.state = States::Idle;
                }
                else
                {

                    // handle inputs
                    if (auto mouse_delta = inputs.get_mouse_delta())
                    {
                        auto up    = float(mouse_delta->y);
                        auto right = -1.0f * float(mouse_delta->x);

                        input_camera.theta += (CAMERA_ROTATE_SPEED * delta_t) * right;

                        constexpr auto low  = -179.0f;
                        constexpr auto high = 0.0f;
                        if (low <= input_camera.phi && input_camera.phi < high)
                        {
                            input_camera.phi += (CAMERA_ROTATE_SPEED * delta_t) * up;
                            input_camera.phi = std::clamp(input_camera.phi, low, high - 1.0f);
                        }
                    }
                }
                break;
            }
            case States::Zoom:
            {
                if (!camera_active || camera_move || camera_orbit)
                {
                    input_camera.state = States::Idle;
                }
                else
                {

                    // handle inputs
                    if (auto scroll = inputs.get_scroll_this_frame())
                    {
                        input_camera.r += (CAMERA_SCROLL_SPEED * delta_t) * scroll->y;
                        input_camera.r = std::max(input_camera.r, 0.1f);
                    }
                }
                break;
            }
        case States::Count: break;
        }
    });

    // Apply Input camera to Transform system
    world.for_each<TransformComponent, InputCameraComponent>([&](auto &transform, const auto &input_camera) {

        auto r         = input_camera.r;
        auto theta_rad = to_radians(input_camera.theta);
        auto phi_rad   = to_radians(input_camera.phi);

        auto spherical_coords = float3(r * std::sin(phi_rad) * std::sin(theta_rad),
                                       r * std::cos(phi_rad),
                                       r * std::sin(phi_rad) * std::cos(theta_rad));

        transform.position = input_camera.target + spherical_coords;


        transform.front = float3(-1.0f * std::sin(phi_rad) * std::sin(theta_rad),
                                 -1.0f * std::cos(phi_rad),
                                 -1.0f * std::sin(phi_rad) * std::cos(theta_rad));

        transform.up = float3(std::sin(PI / 2 + phi_rad) * std::sin(theta_rad),
                              std::cos(PI / 2 + phi_rad),
                              std::sin(PI / 2 + phi_rad) * std::cos(theta_rad));
    });

    // Update view matrix system
    world.for_each<TransformComponent, CameraComponent, InputCameraComponent>([](const auto &transform,
                                                                               auto &camera,
                                                                               const auto &input_camera) {
        camera.view = camera::look_at(transform.position, input_camera.target, float3_UP, &camera.view_inverse);
        // projection will be updated in the renderer
    });
}

void Scene::import_model(Handle<gltf::Model> model_handle)
{
    const auto &model = *models.get(model_handle);
    usize images_offset = images.size();
    images.reserve(model.images.size());

    for (usize i_image = 0; i_image < model.images.size(); i_image += 1)
    {
        images.push_back({
                .data = model.images[i_image].data,
                .srgb = model.images[i_image].srgb
            });
    }

    for (usize i_node = 0; i_node < model.nodes.size(); i_node += 1)
    {
        const auto &node = model.nodes[i_node];
        if (!node.mesh) {
            continue;
        }

        auto transform = model.cached_transforms[i_node];

        for (auto i_primitive : model.meshes[*node.mesh].primitives)
        {
            const auto &primitive = model.primitives[i_primitive];
            const auto &material = model.materials[primitive.material];

            Material new_mat                   = {};
            new_mat.base_color_factor          = material.base_color_factor;
            new_mat.emissive_factor            = material.emissive_factor;
            new_mat.metallic_factor            = material.metallic_factor;
            new_mat.roughness_factor           = material.roughness_factor;
            new_mat.base_color_texture         = images_offset + material.base_color_texture;
            new_mat.normal_texture             = images_offset + material.normal_texture;
            new_mat.metallic_roughness_texture = images_offset + material.metallic_roughness_texture;

            u32 i_material = 0;
            for (; i_material < materials.size(); i_material += 1) {
                if (materials[i_material] == new_mat) {
                    break;
                }
            }

            if (i_material >= materials.size())
            {
                i_material = materials.size();
                materials.push_back(new_mat);
            }

            Mesh new_mesh = {};
            assert(primitive.mode == gltf::RenderingMode::Triangles);
            new_mesh.topology = vulkan::PrimitiveTopology::TriangleList;

            new_mesh.vertex_offset = vertices.size();
            new_mesh.index_offset = indices.size();

            new_mesh.index_count = primitive.index_count;

            // get unique indices
            std::unordered_map<u32, u32> primitive_indices;
            for (usize i_index = primitive.first_index; i_index < primitive.first_index + primitive.index_count; i_index += 1)
            {
                primitive_indices[model.indices[i_index]] = 0;
            }

            // copy over each unique vertex and map the old indices to the global vertex array
            new_mesh.vertex_count = primitive_indices.size();
            vertices.reserve(new_mesh.vertex_count);
            for (auto &[old_index, new_index] : primitive_indices)
            {
                const auto &vertex = model.vertices[old_index];
                primitive_indices[old_index] = vertices.size();
                vertices.push_back({
                        .position = vertex.position,
                        .normal = vertex.normal,
                        .uv0    = vertex.uv0,
                        .color0 = vertex.color0,
                    });
            }

            // add each index pointing to the global vertex array
            for (usize i_index = primitive.first_index; i_index < primitive.first_index + primitive.index_count; i_index += 1)
            {
                indices.push_back(primitive_indices[model.indices[i_index]]);
            }

            auto mesh_handle = meshes.add(new_mesh);

            world.create_entity(std::string_view{"Mesh"}, LocalToWorldComponent{transform}, RenderMeshComponent{mesh_handle, i_material});
        }
    }
}

void Scene::display_ui(UI::Context &ui)
{
    draw_gizmo(world, main_camera);

    world.display_ui(ui);

    static Option<ECS::EntityId> selected_entity;
    const auto display_component = []<ECS::Componentable Component>(ECS::World &world, ECS::EntityId entity) {
        auto *component = world.get_component<Component>(entity);
        if (component)
        {
            ImGui::Separator();
            ImGui::TextUnformatted(Component::type_name());
            ImGui::Separator();
            component->display_ui();
            ImGui::Spacing();
        }
    };

    if (ui.begin_window("Scene"))
    {
        if (ImGui::Button("Load model"))
        {
            auto file_path = platform::file_dialog({{"glTF model", "*.gltf"}});
            if (file_path)
            {
                auto handle = models.add(gltf::load_model(file_path.value()));
                import_model(handle);
            }
        }

        for (auto& [entity, _] : world.entity_index)
        {
            if (world.is_component(entity)) { continue; }

            const char *tag = "";
            if (const auto *internal_id = world.get_component<ECS::InternalId>(entity))
            {
                tag = internal_id->tag;
            }

            auto formatted_name = fmt::format("{}##{}", tag, entity.raw);
            bool is_selected = selected_entity && *selected_entity == entity;
            if (ImGui::Selectable(formatted_name.c_str(), &is_selected))
            {
                selected_entity = entity;
            }
        }

        ui.end_window();
    }

    if (ui.begin_window("Inspector"))
    {
        if (selected_entity)
        {
            const char *tag = "<No name>";
            if (const auto *internal_id = world.get_component<ECS::InternalId>(*selected_entity))
            {
                tag = internal_id->tag;
            }
            ImGui::Text("Selected: %s", tag);

            display_component.template operator()<TransformComponent>(world, *selected_entity);
            display_component.template operator()<CameraComponent>(world, *selected_entity);
            display_component.template operator()<InputCameraComponent>(world, *selected_entity);
            display_component.template operator()<SkyAtmosphereComponent>(world, *selected_entity);
            display_component.template operator()<RenderMeshComponent>(world, *selected_entity);

            {
            auto *component = world.get_component<LocalToWorldComponent>(*selected_entity);
            if (component)
            {
                ImGui::Separator();
                ImGui::TextUnformatted(LocalToWorldComponent::type_name());
                ImGui::Separator();
                auto &transform = component->transform;
                ImGui::SliderFloat3("Translation", transform.col(3).data(), -100.0f, 100.0f);
                ImGui::Spacing();
            }
            }
        }
        ui.end_window();
    }
}
