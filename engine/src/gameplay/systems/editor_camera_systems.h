#pragma once

#include "gameplay/system.h"

class Inputs;

struct CameraComponent;
struct EditorCameraComponent;
struct CameraInputComponent;

struct EditorCameraInputSystem : LocalSystem
{
    EditorCameraInputSystem(const Inputs *_inputs);

    virtual void update(const UpdateContext &ctx) override;

    virtual void register_component(BaseComponent *component) override;
    virtual void unregister_component(BaseComponent *component) override;

private:
    CameraInputComponent *camera_input_component = {};
    EditorCameraComponent *editor_camera_component = {};
    const Inputs *inputs = {};
};

struct EditorCameraTransformSystem : LocalSystem
{
    EditorCameraTransformSystem();
    virtual void update(const UpdateContext &ctx) override;

    virtual void register_component(BaseComponent *component) override;
    virtual void unregister_component(BaseComponent *component) override;

private:
    CameraInputComponent *camera_input_component = {};
    EditorCameraComponent *editor_camera_component = {};
    CameraComponent *camera_component = {};
};
