#include "render/render_world_system.h"

#include "gameplay/update_stages.h"
#include "gameplay/components/mesh_component.h"
#include "gameplay/components/camera_component.h"

#include <exo/logger.h>

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
    logger::info("Preparing {} entities.\n", entities.size());


    ASSERT(main_camera != nullptr);
    {
    ZoneNamedN(camera_update, "Camera update", true);
    // TODO: depend on ui to get the correct aspect ratio
    float aspect_ratio = 1.0f;
    main_camera->set_perspective(aspect_ratio);
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
