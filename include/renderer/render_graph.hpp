#pragma once
#include "types.hpp"
#include <unordered_map>
#include <vulkan/vulkan.h>
#include <vector>
#include <optional>
#include <functional>
#include <vulkan/vulkan_core.h>

/**

   Z prepass:
   depth output:
       "depth buffer"

   Voxelization:
   storage output:
       "voxels albedo"
       "voxels normal"

   Voxel direct lighting:
   texture sampled input:
       "voxels albedo"
       "voxels normal"
   storage output:
       "voxels radiance"

   Voxel aniso mipmaps:
   texture sampled input:
       "voxels radiance"
   storage output:
       "voxels aniso base"

   Voxel directional volumes:
   texture input:
       "voxels aniso base"
   storage output:
       "voxels directional volumes"

   Draw floor:
   color attachment:
       "hdr buffer"
   depth output:
       "depth buffer"

   Draw glTF
   texture sampled input:
       "voxels radiance"
       "voxels directional volumes"
   color attachment:
       "hdr buffer"
   depth output:
       "depth buffer"

   Visualize voxels
   texture sampled input:
       "voxels albedo"
       "voxels normal"
       "voxels radiance"
       "voxels directional volumes"
   color attachment:
       "hdr buffer"
   depth output:
       "depth buffer"

   Render Transmittance LUT
   color attachment:
       "Transmittance LUT"

   Render MultiScattering LUT
   texture sampled input:
       "Transmittance LUT"
   color attachment:
       "MultiScattering LUT"

   Render SkyView LUT
   texture sampled input:
       "Transmittance LUT"
       "MultiScattering LUT"
   color attachment:
       "SkyView LUT"

   Render Sky
   texture sampled input:
       "Transmittance LUT"
       "MultiScattering LUT"
       "SkyView LUT"
   color attachment:
       "hdr buffer"

   Tonemapping
   texture sampled input:
       "hdr buffer"
   color attachment:
       "swapchain image"

   ImGui
   texture sampled input:
       "imgui atlas"
   color attachment:
       "swapchain image"

 **/

namespace my_app
{
namespace vulkan
{
struct API;
struct Image;
struct RenderTarget;
using ImageH = Handle<Image>;
using RenderTargetH = Handle<RenderTarget>;
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
};

enum struct SizeType
{
    Absolute,
    SwapchainRelative
};

// almost same fields as vulkan::ImageInfo
struct ImageDesc
{
    std::string_view name = "No name";
    SizeType size_type    = SizeType::SwapchainRelative;
    float3 size           = float3(1.0f);
    VkImageType type      = VK_IMAGE_TYPE_2D;
    VkFormat format       = VK_FORMAT_R8G8B8A8_UNORM;
    uint samples          = 1;
    uint levels           = 1;
    uint layers           = 1;
};

struct ImageResource
{
    RenderResource resource;
    vulkan::ImageH resolved_img;
    vulkan::RenderTargetH resolved_rt;
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
    std::vector<ImageDescH> combined_sampler_images;
    std::vector<ImageDescH> storage_images;

    // outputs
    std::optional<ImageDescH> color_attachment;
    std::optional<ImageDescH> depth_attachment;

    std::function<void(RenderGraph&, RenderPass&, vulkan::API&)> exec;
};

struct RenderGraph
{
    static RenderGraph create(vulkan::API &api);
    void destroy();

    void clear();
    void add_pass(RenderPass&&);

    void execute();

    vulkan::API *p_api;

    Pool<RenderPass> passes;
    ImageDescH output;

    // swapchain shit
    ImageDescH swapchain;

    Pool<ImageDesc> image_descs;
    std::unordered_map<ImageDescH, ImageResource> images;
};
} // namespace my_app

    /**

   ImGui pass:


    how to specify external textures (e.g gltf textures?)

    graph.add_pass({
        .type = PassType::Graphics,
        .sampled_images = { texture_a, texture_b, shadow_map },
        .color_attachments = { hdr_buffer },
        .depth = depth_buffer,
        .exec = [this, data](RenderGraph& graph, API &api)
        {
            // ImageDesc -> vulkan::ImageH ?
            draw_this();
            draw_that();
        }
    });

     **/
