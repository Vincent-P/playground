#pragma once

#include "render/vulkan/context.hpp"

namespace gfx = vulkan;

inline constexpr uint FRAME_QUEUE_LENGTH = 2;

namespace gltf {struct Model;}

struct RenderMesh
{
    Handle<gltf::Model> model_handle;
    Vec<Handle<gfx::Image>> images;
    u32 vertices_offset;
    u32 indices_offset;
    u32 images_offset;
};

class Scene;

struct Renderer
{
    gfx::Context context;

    uint frame_count;

    Handle<gfx::RenderPass> swapchain_clear_renderpass;
    Handle<gfx::Framebuffer> swapchain_framebuffer;

    // ImGuiPass
    Handle<gfx::GraphicsProgram> gui_program;

    Handle<gfx::Image> gui_font_atlas;
    u32 font_atlas_binding;
    Handle<gfx::Buffer> gui_font_atlas_staging;

    Handle<gfx::Buffer> gui_vertices;
    Handle<gfx::Buffer> gui_indices;
    Handle<gfx::Buffer> gui_options;

    // Command submission
    std::array<gfx::WorkPool, FRAME_QUEUE_LENGTH> work_pools;

    gfx::Fence fence;
    gfx::Fence transfer_done;
    u64 transfer_fence_value = 0;

    /// ---

    static Renderer create(const platform::Window *window);
    void destroy();

    void on_resize();
    bool start_frame();
    bool end_frame(gfx::ComputeWork &cmd);
    void update(const Scene &scene);
};
