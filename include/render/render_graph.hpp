#pragma once
#include "base/types.hpp"
#include "base/pool.hpp"
#include <unordered_map>
#include <vulkan/vulkan.h>
#include <vector>
#include <optional>
#include <functional>
#include <vulkan/vulkan_core.h>

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
using ImageResourceH = Handle<ImageResource>;

// Where a resource is used in the graph
struct RenderResource
{
    std::vector<RenderPassH> sampled_images_in;
    std::vector<RenderPassH> combined_sampler_images_in;
    std::vector<RenderPassH> storage_images_in;
    std::vector<RenderPassH> color_attachment_in;
    std::vector<RenderPassH> depth_attachment_in;
    std::vector<RenderPassH> transfer_dst_in;
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
    Compute,
    BlitToSwapchain
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

    // outputs
    std::vector<ImageDescH> color_attachments;
    VkSampleCountFlagBits samples;
    std::optional<ImageDescH> depth_attachment;

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

    void clear();
    void add_pass(RenderPass&&);
    bool execute();

    vulkan::ImageH get_resolved_image(ImageDescH desc_h) const;

    vulkan::API *p_api;

    ImageDescH swapchain;
    Pool<RenderPass> passes;
    Pool<ImageDesc> image_descs;
    std::unordered_map<ImageDescH, ImageResource> images;
    int render_width;
    int render_height;
};
} // namespace my_app