#include "render/render_world_system.h"

#include "gameplay/update_stages.h"
#include "gameplay/components/mesh_component.h"
#include "gameplay/components/camera_component.h"

#include <exo/logger.h>
#include <imgui/imgui.h>

PrepareRenderWorld::PrepareRenderWorld()
{
    update_stage = UpdateStages::FrameEnd;
    priority_per_stage[update_stage] = 1.0f;
}

void PrepareRenderWorld::initialize(const SystemRegistry &)
{
}

void PrepareRenderWorld::shutdown()
{
}

void PrepareRenderWorld::update(const UpdateContext&)
{
    ZoneScoped;
    ImGui::Text("[PrepareRenderWorld] %zu entities", entities.size());

    // -- }Reset the render world
    render_world.drawable_instances.clear();


    // -- Fill the render world with data from the scene

    ASSERT(main_camera != nullptr);
    {
    // TODO: depend on ui to get the correct aspect ratio
    float aspect_ratio = 1.0f;
    main_camera->set_perspective(aspect_ratio);
    }

    render_world.main_camera_view                = main_camera->get_view();
    render_world.main_camera_projection          = main_camera->get_projection();
    render_world.main_camera_view_inverse        = main_camera->get_view_inverse();
    render_world.main_camera_projection_inverse  = main_camera->get_projection_inverse();

    for (auto &[p_entity, mesh_component] : entities)
    {
        render_world.drawable_instances.emplace_back();
        auto &new_drawable = render_world.drawable_instances.back();

        new_drawable.mesh_asset      = mesh_component->mesh_asset;
        new_drawable.world_transform = mesh_component->get_world_transform();
        new_drawable.world_bounds    = mesh_component->get_world_bounds();
    }
}

void PrepareRenderWorld::register_component(const Entity *entity, BaseComponent *component)
{
    if (auto mesh_component = dynamic_cast<MeshComponent*>(component))
    {
        entities[entity] = mesh_component;
    }
    if (auto camera_component = dynamic_cast<CameraComponent*>(component))
    {
        main_camera = camera_component;
    }
}

void PrepareRenderWorld::unregister_component(const Entity *entity, BaseComponent *component)
{
    if (dynamic_cast<MeshComponent*>(component))
    {
        entities.erase(entity);
    }
}
