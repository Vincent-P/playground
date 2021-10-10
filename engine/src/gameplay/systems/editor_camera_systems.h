#pragma once

#include "gameplay/system.h"

class Inputs;

struct CameraComponent;
struct EditorCameraComponent;
struct CameraInputComponent;

struct EditorCameraInputSystem : LocalSystem
{
    EditorCameraInputSystem(const Inputs *_inputs);

    void update(const UpdateContext &ctx) final;

    void register_component(BaseComponent *component) final;
    void unregister_component(BaseComponent *component) final;

private:
    CameraInputComponent *camera_input_component = {};
    EditorCameraComponent *editor_camera_component = {};
    const Inputs *inputs = {};
};

struct EditorCameraTransformSystem : LocalSystem
{
    EditorCameraTransformSystem();
    void update(const UpdateContext &ctx) final;

    void register_component(BaseComponent *component) final;
    void unregister_component(BaseComponent *component) final;

private:
    CameraInputComponent *camera_input_component = {};
    EditorCameraComponent *editor_camera_component = {};
    CameraComponent *camera_component = {};
};
