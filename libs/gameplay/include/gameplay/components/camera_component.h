#pragma once

#include "gameplay/component.h"
#include <exo/option.h>

struct CameraComponent : SpatialComponent
{
	float near_plane = 0.1f;
	float far_plane  = 100000.0f;
	float fov        = 90.0f;

	void look_at(float3 eye, float3 at, float3 up);

	// clang-format off
    float4x4 get_view() const { return view; }
    float4x4 get_view_inverse() const { return view_inverse; }
    float4x4 get_projection() const { return projection; }
    float4x4 get_projection_inverse() const { return projection_inverse; }
	// clang-format on

private:
	float4x4 view;
	float4x4 view_inverse;
	float4x4 projection;
	float4x4 projection_inverse;
};

enum struct EditorCameraState : uint
{
	Idle,
	Move,
	Orbit,
	Zoom,
	Count
};

struct EditorCameraComponent : BaseComponent
{
	EditorCameraState state = EditorCameraState::Idle;

	// spherical coordinates: radius r, azymuthal angle theta, polar angle phi
	float  r      = 6.0f;
	float  theta  = -78.0f;
	float  phi    = -65.0f;
	float3 target = {0.0f};
};

struct CameraInputComponent : BaseComponent
{
	bool         camera_active = false;
	bool         camera_move   = false;
	bool         camera_orbit  = false;
	int2         scroll        = {};
	Option<int2> mouse_delta   = {};
	float        aspect_ratio  = {};
};
