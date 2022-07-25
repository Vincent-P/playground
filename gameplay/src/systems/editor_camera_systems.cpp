#include "gameplay/systems/editor_camera_systems.h"

#include "gameplay/component.h"
#include "gameplay/components/camera_component.h"
#include "gameplay/inputs.h"
#include "gameplay/update_context.h"

#include <exo/logger.h>
#include <exo/maths/vectors.h>

#include <algorithm>

EditorCameraInputSystem::EditorCameraInputSystem(const Inputs *_inputs) : inputs{_inputs}
{
	ASSERT(inputs != nullptr);
	update_stage                     = UpdateStages::Input;
	priority_per_stage[update_stage] = 1.0f;
}

void EditorCameraInputSystem::register_component(BaseComponent *component)
{
	if (auto *ci = dynamic_cast<CameraInputComponent *>(component)) {
		camera_input_component = ci;
	} else if (auto *ec = dynamic_cast<EditorCameraComponent *>(component)) {
		editor_camera_component = ec;
	}
}

void EditorCameraInputSystem::unregister_component(BaseComponent *component)
{
	if (component == camera_input_component) {
		camera_input_component = nullptr;
	} else if (component == editor_camera_component) {
		editor_camera_component = nullptr;
	}
}

EditorCameraTransformSystem::EditorCameraTransformSystem()
{
	update_stage                     = UpdateStages::PrePhysics;
	priority_per_stage[update_stage] = 1.0f;
}

void EditorCameraTransformSystem::register_component(BaseComponent *component)
{
	if (auto *ci = dynamic_cast<CameraInputComponent *>(component)) {
		camera_input_component = ci;
	} else if (auto *ec = dynamic_cast<EditorCameraComponent *>(component)) {
		editor_camera_component = ec;
	} else if (auto *c = dynamic_cast<CameraComponent *>(component)) {
		camera_component = c;
	}
}

void EditorCameraTransformSystem::unregister_component(BaseComponent *component)
{
	if (component == camera_input_component) {
		camera_input_component = nullptr;
	} else if (component == editor_camera_component) {
		editor_camera_component = nullptr;
	} else if (component == camera_component) {
		camera_component = nullptr;
	}
}

void EditorCameraInputSystem::update(const UpdateContext &)
{
	ASSERT(camera_input_component && editor_camera_component);

	camera_input_component->camera_active = inputs->is_pressed(Action::CameraModifier);
	camera_input_component->camera_move   = inputs->is_pressed(Action::CameraMove);
	camera_input_component->camera_orbit  = inputs->is_pressed(Action::CameraOrbit);

	if (auto scroll = inputs->get_scroll_this_frame()) {
		camera_input_component->scroll = scroll.value();
	} else {
		camera_input_component->scroll = {};
	}
	camera_input_component->mouse_delta  = inputs->get_mouse_delta();
	f32 aspect_ratio                     = f32(inputs->main_window_size.x) / f32(inputs->main_window_size.y);
	camera_input_component->aspect_ratio = aspect_ratio;
}

void EditorCameraTransformSystem::update(const UpdateContext &ctx)
{
	ASSERT(editor_camera_component && camera_component);

	constexpr float CAMERA_MOVE_SPEED   = 5.0f;
	constexpr float CAMERA_ROTATE_SPEED = 80.0f;
	constexpr float CAMERA_SCROLL_SPEED = 80.0f;

	const auto camera_active = camera_input_component->camera_active;
	const auto camera_move   = camera_input_component->camera_move;
	const auto camera_orbit  = camera_input_component->camera_orbit;
	const auto camera_scroll = camera_input_component->scroll;
	const auto mouse_delta   = camera_input_component->mouse_delta;

	auto &state = editor_camera_component->state;

	// TODO: transition table DSL?
	// state transition
	switch (editor_camera_component->state) {
	case EditorCameraState::Idle: {
		if (camera_active && camera_input_component->camera_move) {
			state = EditorCameraState::Move;
		} else if (camera_active && camera_orbit) {
			state = EditorCameraState::Orbit;
		} else if (camera_active) {
			state = EditorCameraState::Zoom;
		} else {
			editor_camera_component->target.y +=
				(CAMERA_SCROLL_SPEED * static_cast<float>(ctx.delta_t)) * static_cast<float>(camera_scroll.y);
		}

		break;
	}
	case EditorCameraState::Move: {
		if (!camera_active || !camera_move) {
			state = EditorCameraState::Idle;
		} else {
			// handle inputs
			if (mouse_delta) {
				auto mouse_up    = float(mouse_delta->y);
				auto mouse_right = float(mouse_delta->x);

				auto   camera_world = reinterpret_cast<SpatialComponent *>(camera_component)->get_local_transform();
				float3 front        = -1.0 * normalize(camera_world.col(2).xyz());
				float3 up           = normalize(camera_world.col(1).xyz());

				auto camera_plane_forward = normalize(float3(front.x, 0.0f, front.z));
				auto camera_right         = cross(up, front);
				auto camera_plane_right   = normalize(float3(camera_right.x, 0.0f, camera_right.z));

				editor_camera_component->target =
					editor_camera_component->target +
					CAMERA_MOVE_SPEED * static_cast<float>(ctx.delta_t) * mouse_right * camera_plane_right;
				editor_camera_component->target =
					editor_camera_component->target +
					CAMERA_MOVE_SPEED * static_cast<float>(ctx.delta_t) * mouse_up * camera_plane_forward;
			}
		}
		break;
	}
	case EditorCameraState::Orbit: {
		if (!camera_active || !camera_orbit) {
			state = EditorCameraState::Idle;
		} else {

			// handle inputs
			if (mouse_delta) {
				auto up    = float(mouse_delta->y);
				auto right = -1.0f * float(mouse_delta->x);

				editor_camera_component->theta += (CAMERA_ROTATE_SPEED * static_cast<float>(ctx.delta_t)) * right;

				constexpr auto low  = -179.0f;
				constexpr auto high = 0.0f;
				if (low <= editor_camera_component->phi && editor_camera_component->phi < high) {
					editor_camera_component->phi += (CAMERA_ROTATE_SPEED * static_cast<float>(ctx.delta_t)) * up;
					editor_camera_component->phi = std::clamp(editor_camera_component->phi, low, high - 1.0f);
				}
			}
		}
		break;
	}
	case EditorCameraState::Zoom: {
		if (!camera_active || camera_move || camera_orbit) {
			state = EditorCameraState::Idle;
		} else {
			editor_camera_component->r +=
				(CAMERA_SCROLL_SPEED * static_cast<float>(ctx.delta_t)) * static_cast<float>(camera_scroll.y);
			editor_camera_component->r = std::max(editor_camera_component->r, 0.1f);
		}
		break;
	}
	case EditorCameraState::Count:
		break;
	}

	auto r         = editor_camera_component->r;
	auto theta_rad = exo::to_radians(editor_camera_component->theta);
	auto phi_rad   = exo::to_radians(editor_camera_component->phi);

	auto spherical_coords = float3(r * std::sin(phi_rad) * std::sin(theta_rad),
		r * std::cos(phi_rad),
		r * std::sin(phi_rad) * std::cos(theta_rad));

	auto position = editor_camera_component->target + spherical_coords;
	auto front    = float3(-1.0f * std::sin(phi_rad) * std::sin(theta_rad),
        -1.0f * std::cos(phi_rad),
        -1.0f * std::sin(phi_rad) * std::cos(theta_rad));
	auto up       = float3(std::sin(PI / 2 + phi_rad) * std::sin(theta_rad),
        std::cos(PI / 2 + phi_rad),
        std::sin(PI / 2 + phi_rad) * std::cos(theta_rad));
	auto right    = cross(front, up);

	float4x4 new_transform = {};
	new_transform.col(0)   = float4(right, 0.0f);
	new_transform.col(1)   = float4(up, 0.0f);
	new_transform.col(2)   = float4(-1.0f * front, 0.0f);
	new_transform.col(3)   = float4(position, 1.0f);

	reinterpret_cast<SpatialComponent *>(camera_component)->set_local_transform(new_transform);

	camera_component->look_at(position, editor_camera_component->target, up);
	camera_component->set_perspective(camera_input_component->aspect_ratio);
}
