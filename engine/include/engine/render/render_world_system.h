#pragma once
#include <exo/collections/map.h>

#include "engine/gameplay/system.h"
#include "engine/render/render_world.h"

struct RenderWorld;
struct MeshComponent;
struct CameraComponent;

struct PrepareRenderWorld : GlobalSystem
{
    PrepareRenderWorld();

    void initialize(const SystemRegistry &) final;
    void shutdown() final;

    void update(const UpdateContext&) final;

    void register_component(const Entity *entity, BaseComponent *component) final;
    void unregister_component(const Entity *entity, BaseComponent *component) final;

    RenderWorld render_world;

private:
    CameraComponent* main_camera;
    exo::Map<const Entity*, MeshComponent*> entities;
};