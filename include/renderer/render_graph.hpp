#pragma once
#include "types.hpp"
#include <vulkan/vulkan.h>
#include <vector>
#include <optional>
#include <functional>

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
};

struct RenderPass;
struct RenderGraph;

enum struct SizeType
{
    Absolute,
    SwapchainRelative
};

struct ImageDesc
{
    SizeType size_type;
    float3 size;
    VkImageType type;
    VkFormat format;
    uint samples;
    uint levels;
    uint layers;
};
using ImageDescH = Handle<ImageDesc>;

enum struct PassType
{
    Graphics,
    Compute
    // Transfer?
};


using RenderPassH = Handle<RenderPass>;
struct RenderPass
{
    PassType type;

    std::function<void(RenderGraph&, vulkan::API&)> exec;

    // params? (shader dynamic uniform buffer)

    // inputs
    std::vector<ImageDescH> sampled_images;
    std::vector<ImageDescH> combined_sampler_images;
    std::vector<ImageDescH> storage_images;

    // outputs
    std::vector<ImageDescH> color_attachments;
    std::optional<ImageDescH> depth_attachment;
};

struct RenderGraph
{
    /**

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
    void add_pass(RenderPass&&);

    Arena<RenderPassH> passes;

    Arena<ImageDesc> color_attachments;
    Arena<ImageDesc> sampled_images;
    Arena<ImageDesc> combined_sampler_images;
    Arena<ImageDesc> storage_images;
    // or
    Arena<ImageDesc> images;
};
} // namespace my_app
