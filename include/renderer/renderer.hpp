#pragma once
#include <string>
#include "camera.hpp"
#include "gltf.hpp"
#include "renderer/hl_api.hpp"
#include "renderer/vlk_context.hpp"

/***
 * The renderer is the orchestrator of the Vulkan Context and the HL API.
 * The main functions are StartFrame and EndFrame and it contains
 * raw HL API calls in between to draw things or validate/cook a Render Graph.
 ***/

namespace my_app
{
struct Model;
struct Event;

struct VoxelDebug
{
    float3 center{-18.0f, -1.0f, -11.0f};
    float size{0.1f};
    uint res{256};
};

struct Renderer
{
    vulkan::API api;

    static Renderer create(const Window &window, Camera &camera);
    void destroy();

    void draw();
    void on_resize(int width, int height);
    void wait_idle();

    void reload_shader(const char* prefix_path, const Event& shader_event);

    // glTF
    void load_model_data();
    void draw_model();
    void destroy_model();

    // ImGui
    void imgui_draw();

    void voxelize_scene();
    void inject_direct_lighting();
    void visualize_voxels();

    // ImGui
    vulkan::GraphicsProgramH gui_program;
    vulkan::ImageH gui_texture;

    // glTF
    Model model;

    // Shadow Map
    Camera sun;
    vulkan::GraphicsProgramH model_vertex_only;
    vulkan::RenderTargetH shadow_map_depth_rt;

    // Voxelization
    VoxelDebug voxel_options{};
    vulkan::GraphicsProgramH voxelization;
    vulkan::GraphicsProgramH visualization;
    vulkan::ComputeProgramH inject_radiance;

    vulkan::SamplerH voxels_sampler;
    vulkan::ImageH voxels_albedo;
    vulkan::ImageH voxels_normal;
    vulkan::ImageH voxels_radiance;

    vulkan::RenderTargetH depth_rt;
    vulkan::RenderTargetH color_rt;
    Camera *p_camera;
};

} // namespace my_app
