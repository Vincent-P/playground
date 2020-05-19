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
    float3 center{-13.0f, -1.0f, -11.0f};
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
    void generate_aniso_voxels();
    void visualize_voxels();


    // ImGui
    vulkan::GraphicsProgramH gui_program;
    vulkan::ImageH gui_texture;

    // glTF
    Model model;

    // Shadow Map
    Camera sun;
    vulkan::GraphicsProgramH model_depth_only;

    // Voxelization
    VoxelDebug voxel_options{};
    vulkan::GraphicsProgramH voxelization;
    vulkan::GraphicsProgramH visualization;
    vulkan::ComputeProgramH inject_radiance;
    vulkan::ComputeProgramH generate_aniso_base;
    vulkan::ComputeProgramH generate_aniso_mipmap;

    vulkan::SamplerH voxels_sampler;
    vulkan::ImageH voxels_albedo;
    vulkan::ImageH voxels_normal;
    vulkan::ImageH voxels_radiance;
    std::vector<vulkan::ImageH> voxels_directional_volumes;

    vulkan::RenderTargetH depth_rt;
    vulkan::RenderTargetH color_rt;
    Camera *p_camera;
};

} // namespace my_app
