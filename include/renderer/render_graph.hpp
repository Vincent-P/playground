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
struct Image;
using ImageH = Handle<Image>;
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
    std::vector<vulkan::ImageH> external_images;
    std::vector<ImageDescH> combined_sampler_images;
    std::vector<ImageDescH> storage_images;

    // outputs
    std::vector<ImageDescH> color_attachments;
    std::optional<ImageDescH> depth_attachment;
};

struct RenderGraph
{
    /**

   ImGui pass:
    std::vector<vulkan::ImageH> external_images; // external are always sampled?

    external_images.push_back(font_atlas);

    for (int list = 0; list < data->CmdListsCount; list++) {
        const auto &cmd_list = data->CmdLists[list];

        for (int command_index = 0; command_index < cmd_list.CmdBuffer.Size; command_index++) {
            const auto &draw_command = &cmd_list.CmdBuffer[command_index];

            if (draw_command.TextureId) {
                auto image_h = vulkan::ImageH(static_cast<u32>(reinterpret_cast<u64>(draw_command->TextureId)));
                external_images.push_back(image_h);
            }
        }
    }

    graph.add_pass({
        .type = PassType::Graphics,
        .external_images = external_images,
        .color_attachments = { swapchain },
        .exec = [this, data](RenderGraph& graph, API &api)
        {
            auto font_atlas = external_images[0];
            usize current_external_image = 1;

            /// --- Prepare index and vertex buffer
            auto v_pos = api.dynamic_vertex_buffer(sizeof(ImDrawVert) * static_cast<u32>(data->TotalVtxCount));
            auto i_pos = api.dynamic_index_buffer(sizeof(ImDrawIdx) * static_cast<u32>(data->TotalIdxCount));

            auto *vertices = reinterpret_cast<ImDrawVert *>(v_pos.mapped);
            auto *indices  = reinterpret_cast<ImDrawIdx *>(i_pos.mapped);

            for (int i = 0; i < data->CmdListsCount; i++) {
                const auto &cmd_list = data->CmdLists[i];

                std::memcpy(vertices, cmd_list.VtxBuffer.Data, sizeof(ImDrawVert) * size_t(cmd_list->VtxBuffer.Size));
                std::memcpy(indices, cmd_list.IdxBuffer.Data, sizeof(ImDrawIdx) * size_t(cmd_list->IdxBuffer.Size));

                vertices += cmd_list->VtxBuffer.Size;
                indices += cmd_list->IdxBuffer.Size;
            }

            float4 scale_and_translation;
            scale_and_translation[0] = 2.0f / data->DisplaySize.x;                            // X Scale
            scale_and_translation[1] = 2.0f / data->DisplaySize.y;                            // Y Scale
            scale_and_translation[2] = -1.0f - data->DisplayPos.x * scale_and_translation[0]; // X Translation
            scale_and_translation[3] = -1.0f - data->DisplayPos.y * scale_and_translation[1]; // Y Translation

            // Will project scissor/clipping rectangles into framebuffer space
            ImVec2 clip_off   = data->DisplayPos;       // (0,0) unless using multi-viewports
            ImVec2 clip_scale = data->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

            VkViewport viewport{};
            viewport.width    = data->DisplaySize.x * data->FramebufferScale.x;
            viewport.height   = data->DisplaySize.y * data->FramebufferScale.y;
            viewport.minDepth = 1.0f;
            viewport.maxDepth = 1.0f;
            api.set_viewport(viewport);

            api.bind_vertex_buffer(v_pos);
            api.bind_index_buffer(i_pos);

            /// --- Draws

            enum UIProgram {
                Float,
                Uint
            };

            // Render GUI
            i32 vertex_offset = 0;
            u32 index_offset  = 0;
            for (int list = 0; list < data->CmdListsCount; list++) {
                const auto &cmd_list = data->CmdLists[list];

                for (int command_index = 0; command_index < cmd_list.CmdBuffer.Size; command_index++) {
                    const auto &draw_command = &cmd_list.CmdBuffer[command_index];

                    vulkan::GraphicsProgramH current = gui_program;

                    if (draw_command->TextureId) {
                        auto texture = vulkan::ImageH(static_cast<u32>(reinterpret_cast<u64>(draw_command->TextureId)));
                        auto& image = api.get_image(texture);

                        if (image.info.format == VK_FORMAT_R32_UINT)
                        {
                            current = gui_uint_program;
                        }

                        api.bind_image(current, vulkan::SHADER_DESCRIPTOR_SET, 0, texture);
                    }
                    else {
                        api.bind_image(current, vulkan::SHADER_DESCRIPTOR_SET, 0, gui_texture);
                    }

                    api.bind_program(current);
                    api.push_constant(VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(float4), &scale_and_translation);

                    // Project scissor/clipping rectangles into framebuffer space
                    ImVec4 clip_rect;
                    clip_rect.x = (draw_command->ClipRect.x - clip_off.x) * clip_scale.x;
                    clip_rect.y = (draw_command->ClipRect.y - clip_off.y) * clip_scale.y;
                    clip_rect.z = (draw_command->ClipRect.z - clip_off.x) * clip_scale.x;
                    clip_rect.w = (draw_command->ClipRect.w - clip_off.y) * clip_scale.y;

                    // Apply scissor/clipping rectangle
                    // FIXME: We could clamp width/height based on clamped min/max values.
                    VkRect2D scissor;
                    scissor.offset.x      = (static_cast<i32>(clip_rect.x) > 0) ? static_cast<i32>(clip_rect.x) : 0;
                    scissor.offset.y      = (static_cast<i32>(clip_rect.y) > 0) ? static_cast<i32>(clip_rect.y) : 0;
                    scissor.extent.width  = static_cast<u32>(clip_rect.z - clip_rect.x);
                    scissor.extent.height = static_cast<u32>(clip_rect.w - clip_rect.y + 1); // FIXME: Why +1 here?

                    api.set_scissor(scissor);

                    api.draw_indexed(draw_command->ElemCount, 1, index_offset, vertex_offset, 0);

                    index_offset += draw_command->ElemCount;
                }
                vertex_offset += cmd_list->VtxBuffer.Size;
            }

        }
    });


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
