#pragma once

#include "gameplay/system.h"
#include <reflection/reflection.h>

class Inputs;

struct CameraComponent;
struct EditorCameraComponent;
struct CameraInputComponent;

struct EditorCameraInputSystem : LocalSystem
{
	using Self  = EditorCameraInputSystem;
	using Super = LocalSystem;
	REFL_REGISTER_TYPE_WITH_SUPER("EditorCameraInputSystem")

	CameraInputComponent  *camera_input_component  = {};
	EditorCameraComponent *editor_camera_component = {};
	const Inputs          *inputs                  = {};

	// --
	EditorCameraInputSystem(const Inputs *_inputs);
	void update(const UpdateContext &ctx) final;
	void register_component(refl::BasePtr<BaseComponent> component) final;
	void unregister_component(refl::BasePtr<BaseComponent> component) final;
};

struct EditorCameraTransformSystem : LocalSystem
{
	using Self  = EditorCameraTransformSystem;
	using Super = LocalSystem;
	REFL_REGISTER_TYPE_WITH_SUPER("EditorCameraTransformSystem")

	CameraInputComponent  *camera_input_component  = {};
	EditorCameraComponent *editor_camera_component = {};
	CameraComponent       *camera_component        = {};

	// --
	EditorCameraTransformSystem();
	void update(const UpdateContext &ctx) final;
	void register_component(refl::BasePtr<BaseComponent> component) final;
	void unregister_component(refl::BasePtr<BaseComponent> component) final;
};
