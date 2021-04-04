#pragma once

#include "base/handle.hpp"

#include "render/vulkan/commands.hpp"
#include "render/vulkan/context.hpp"
#include "render/vulkan/device.hpp"
#include "render/vulkan/resources.hpp"
#include "render/vulkan/surface.hpp"

#include "render/streaming_buffer.hpp"
#include "render/bvh.hpp"

namespace gfx = vulkan;

inline constexpr uint FRAME_QUEUE_LENGTH = 1;

namespace gltf {struct Model;}
namespace UI {struct Context;}
struct Mesh;
struct Material;

struct PACKED RenderMeshData
{
    float4x4 transform;
    Handle<Mesh> mesh_handle;
    u32      i_material;
    u32 texture_offset = 0;
};

class Scene;

struct RenderTargets
{
    Handle<gfx::RenderPass> clear_renderpass;
    Handle<gfx::RenderPass> load_renderpass;
    Handle<gfx::Framebuffer> framebuffer;
    Handle<gfx::Image> image;
    Handle<gfx::Image> depth;
};

struct Settings
{
    uint2 render_resolution;
    bool resolution_dirty;
    bool enable_taa = true;
    bool enable_path_tracing = false;
};

struct ImGuiPass
{
    Handle<gfx::GraphicsProgram> program;
    Handle<gfx::Image>  font_atlas;
    Handle<gfx::Buffer> font_atlas_staging;
    Handle<gfx::Buffer> vertices;
    Handle<gfx::Buffer> indices;

    // transfer stuff
    bool should_upload_atlas;
    u64 transfer_done_value = u64_invalid;
};

struct PACKED TonemapOptions
{
    uint sampled_hdr_buffer;
    uint sampled_luminance_output;
    uint storage_output_frame;
    uint selected;
    float exposure;
};

struct TonemapPass
{
    Handle<gfx::ComputeProgram> tonemap;
    Handle<gfx::ComputeProgram> build_histo;
    Handle<gfx::ComputeProgram> average_histo;

    Handle<gfx::Buffer> histogram;
    Handle<gfx::Image> average_luminance;
    TonemapOptions options = {};
};

struct PACKED GlobalUniform
{
    float4x4 camera_view;
    float4x4 camera_proj;
    float4x4 camera_view_inverse;
    float4x4 camera_projection_inverse;
    float4   camera_position;
    u64 vertex_buffer_ptr;
    u64 primitive_buffer_ptr;
    float2   resolution;
    float delta_t;
    u32 frame_count;
    u32 camera_moved;
};

struct PACKED PushConstants
{
    u32 draw_idx = u32_invalid;
    u32 render_mesh_idx = u32_invalid;
};

struct StbImage
{
    int width       = 0;
    int height      = 0;
    u8 *pixels      = nullptr;
    Handle<gfx::Buffer> staging_buffer;
    Handle<gfx::Image> gpu_image;
    int nb_comp     = 0;
    VkFormat format = VK_FORMAT_UNDEFINED;
};


struct Renderer
{
    Settings settings;

    // Base renderer
    gfx::Context context;
    gfx::Device device;
    gfx::Surface surface;
    uint frame_count;
    std::array<gfx::WorkPool, FRAME_QUEUE_LENGTH> work_pools;
    gfx::Fence fence;
    Handle<gfx::Buffer> dynamic_buffer;
    usize dynamic_buffer_offset = 0;
    usize dynamic_buffer_this_frame_size = 0;
    usize dynamic_buffer_last_frame_end = 0;
    usize dynamic_buffer_last_frame_size = 0;
    Handle<gfx::Image> empty_sampled_image;
    Handle<gfx::Image> empty_storage_image;
    Vec<Handle<gfx::Image>> render_textures;
    Vec<StbImage> upload_images;

    // User renderer
    GlobalUniform global_uniform;
    Handle<gfx::Buffer> global_uniform_buffer;
    Handle<gfx::Image> depth_buffer;
    RenderTargets hdr_rt;
    RenderTargets ldr_rt;
    RenderTargets swapchain_rt;

    gfx::Fence transfer_done;

    ImGuiPass imgui_pass;
    TonemapPass tonemap_pass;

    StreamingBuffer vertex_buffer;
    StreamingBuffer index_buffer;
    Handle<gfx::Buffer> material_buffer;
    Handle<gfx::Buffer> material_buffer_staging;
    u32 material_transfer = u32_invalid;

    BVH bvh;
    Handle<gfx::Buffer> bvh_nodes_buffer;
    Handle<gfx::Buffer> bvh_nodes_buffer_staging;
    Handle<gfx::Buffer> bvh_faces_buffer;
    Handle<gfx::Buffer> bvh_faces_buffer_staging;

    u32 bvh_transfer = u32_invalid;

    u64 geometry_transfer_done_value = u64_invalid;

    Handle<gfx::GraphicsProgram> opaque_program;
    Handle<gfx::ComputeProgram> path_tracing_program;
    Handle<gfx::ComputeProgram> taa;
    Handle<gfx::Image> history_buffers[2];
    u32 current_history = 0;

    GpuPool render_mesh_data;
    Vec<u32> render_mesh_indices;

    bool render_mesh_data_dirty = false;

    /// ---

    static Renderer create(const platform::Window &window);
    void destroy();

    void display_ui(UI::Context &ui);
    void update(Scene &scene);

    // Ring buffer uniforms
    std::pair<void*, usize> allocate_uniform(usize len);
    template<typename T>
    std::pair<T*, usize> allocate_uniform()
    {
        auto [void_ptr, offset] = allocate_uniform(sizeof(T));
        return std::make_pair(reinterpret_cast<T*>(void_ptr), offset);
    }

    void *bind_shader_options(gfx::ComputeWork &cmd, Handle<gfx::GraphicsProgram> program, usize options_len);
    void *bind_shader_options(gfx::ComputeWork &cmd, Handle<gfx::ComputeProgram> program, usize options_len);
    template<typename T> T *bind_shader_options(gfx::ComputeWork &cmd, Handle<gfx::GraphicsProgram> program) { return (T*)bind_shader_options(cmd, program, sizeof(T)); }
    template<typename T> T *bind_shader_options(gfx::ComputeWork &cmd, Handle<gfx::ComputeProgram> program) { return (T*)bind_shader_options(cmd, program, sizeof(T)); }

    void reload_shader(std::string_view shader_name);
    void on_resize();
    bool start_frame();
    bool end_frame(gfx::ComputeWork &cmd);
};

inline uint3 dispatch_size(uint3 image_size, uint threads_xy, uint threads_z = 1)
{
    return {
        .x = image_size.x / threads_xy + uint(image_size.x % threads_xy != 0),
        .y = image_size.y / threads_xy + uint(image_size.y % threads_xy != 0),
        .z = image_size.z / threads_z  + uint(image_size.z % threads_z  != 0),
    };
}
