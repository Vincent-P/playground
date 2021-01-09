#pragma once
#include "base/pool.hpp"
#include "base/types.hpp"
#include "base/option.hpp"
#include "render/hl_api.hpp"

#include <cmath>
#include <functional>
#include <map>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>

namespace my_app
{
namespace vulkan
{
struct API;
struct Image;
struct ImageView;
using ImageH = Handle<Image>;
using ImageViewH = Handle<ImageView>;
};

namespace UI
{
    struct Context;
};

struct RenderPass;
struct RenderGraph;
struct RenderResource;
struct ImageDesc;
struct ImageResource;

using ImageDescH = Handle<ImageDesc>;
using RenderPassH = Handle<RenderPass>;

// Where a resource is used in the graph
struct RenderResource
{
    std::vector<RenderPassH> sampled_images_in;
    std::vector<RenderPassH> combined_sampler_images_in;
    std::vector<RenderPassH> storage_images_in;
    std::vector<RenderPassH> color_attachment_in;
    std::vector<RenderPassH> depth_attachment_in;
};

enum struct SizeType
{
    Absolute,
    RenderRelative // relative to render resolution
};

// almost same fields as vulkan::ImageInfo
struct ImageDesc
{
    std::string_view name               = "No name";
    SizeType size_type                  = SizeType::RenderRelative;
    float3 size                         = float3(1.0f);
    VkImageType type                    = VK_IMAGE_TYPE_2D;
    VkFormat format                     = VK_FORMAT_R8G8B8A8_UNORM;
    std::vector<VkFormat> extra_formats = {};
    uint samples                        = 1;
    uint levels                         = 1;
    uint layers                         = 1;

    bool operator==(const ImageDesc &b) const = default;
};

struct ImageResource
{
    RenderResource resource;
    vulkan::ImageH resolved_img;
};

enum struct PassType
{
    Graphics,
    Compute
    // Transfer?
};

struct RenderPass
{
    std::string_view name;
    PassType type;

    // params? (shader dynamic uniform buffer)

    // inputs
    std::vector<vulkan::ImageH> external_images;
    std::vector<ImageDescH> sampled_images;
    std::vector<ImageDescH> storage_images;

    std::vector<vulkan::BufferH> index_buffers;
    std::vector<vulkan::BufferH> vertex_buffers;
    std::vector<vulkan::BufferH> transfer_src_buffers;
    std::vector<vulkan::BufferH> transfer_dst_buffers;
    std::vector<vulkan::BufferH> storage_buffers;

    // outputs
    std::vector<ImageDescH> color_attachments;
    VkSampleCountFlagBits samples;
    Option<ImageDescH> depth_attachment;

    std::function<void(RenderGraph&, RenderPass&, vulkan::API&)> exec;

    // ui?
    bool opened = true;

    bool operator==(const RenderPass &/*b*/) const
    {
        return false;
    }
};

struct RenderGraph
{
    static void create(RenderGraph &render_graph, vulkan::API &api);
    void destroy();

    void on_resize(int render_width, int render_height);
    void display_ui(UI::Context &ui);

    void start_frame();
    void add_pass(RenderPass&&);
    bool execute();

    [[nodiscard]]
    inline vulkan::ImageH get_resolved_image(ImageDescH desc_h) const
    {
        const auto &image = images.at(desc_h);
        assert(image.resolved_img.is_valid());
        return image.resolved_img;
    }

    [[nodiscard]]
    inline uint2 get_image_desc_size(const ImageDesc &desc) const
    {
        float width  = desc.size.x;
        float height = desc.size.y;
        if (desc.size_type == SizeType::RenderRelative)
        {
            width  = std::ceil(width * render_width);
            height = std::ceil(height * render_height);
        }
        return {
            .x = static_cast<u32>(width),
            .y = static_cast<u32>(height)
        };
    }

    vulkan::API *p_api;

    ImageDescH swapchain;
    Pool<RenderPass> passes;
    Pool<ImageDesc> image_descs;
    std::map<ImageDescH, ImageResource> images;
    std::vector<std::tuple<vulkan::ImageInfo, vulkan::ImageH, bool>> cache;
    int render_width;
    int render_height;
};
} // namespace my_app
