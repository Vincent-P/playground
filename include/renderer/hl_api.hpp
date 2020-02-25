#pragma once

#include <vulkan/vulkan.hpp>
#include <thsvs/thsvs_simpler_vulkan_synchronization.h>
#include <vector>
#include "renderer/vlk_context.hpp"
#include "types.hpp"

/***
 * The HL API is a Vulkan abstraction.
 * It contains high-level struct and classes over vulkan
 * - Shaders/Programs: Abstract all the descriptor layouts, bindings, and pipelines manipulation
 * - Render Target: Abstract everything related to the render passes and framebuffer
 * - Textures/Buffers: Abstract resources
 ***/

namespace my_app
{
    namespace vulkan
    {
        struct ImageInfo
        {
            const char* name;
            vk::ImageType type = vk::ImageType::e2D;
            vk::Format format = vk::Format::eR8G8B8A8Unorm;
            u32 width;
            u32 height;
            u32 depth;
            u32 mip_levels = 1;
            u32 layers = 1;
            vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1;
        };

        struct Image
        {
            vk::Image vkhandle;
            vk::ImageCreateInfo image_info;
            VmaAllocation allocation;
            VmaMemoryUsage memory_usage;
            ThsvsAccessType access;
            vk::ImageSubresourceRange full_range;
            vk::ImageView default_view;
        };
        using ImageH = Handle<Image>;

        struct Sampler
        {
            vk::UniqueSampler vkhandle;
        };

        struct BufferInfo
        {
            const char* name;
            usize size;
            vk::BufferUsageFlags usage;
            VmaMemoryUsage memory_usage;
        };

        struct Buffer
        {
            vk::Buffer vkhandle;
            VmaAllocation allocation;
            VmaMemoryUsage memory_usage;
            vk::BufferUsageFlags usage;
            void* mapped;
            usize size;
        };
        using BufferH = Handle<Buffer>;

        struct RTInfo
        {
            bool is_swapchain;
        };

        struct RenderTarget
        {
            bool is_swapchain;
            ImageH image;
        };

        using RenderTargetH = Handle<RenderTarget>;


        struct FrameBuffer
        {
            vk::UniqueFramebuffer vkhandle;
        };

        struct PassInfo
        {
            bool clear; // if the pass should clear the rt or not
            bool present; // if it is the last pass and it should transition to present
            RenderTargetH rt;
        };

        struct RenderPass
        {
            vk::UniqueRenderPass vkhandle;
        };

        // Idea: Program contains different "configurations" coresponding to pipelines so that
        // the HL API has a VkPipeline equivalent used to make sure they are created only during load time?
        // maybe it is possible to deduce these configurations automatically from render graph, but render graph is
        // created every frame

        struct Program
        {
            // descriptor layouts
            //
        };

        using ProgramH = Handle<Program>;

        struct Shader
        {
            vk::UniqueShaderModule vkhandle;
        };

        using ShaderH = Handle<Shader>;

        // temporary command buffer for the frame
        struct CommandBuffer
        {
            Context& ctx;
            vk::UniqueCommandBuffer vkhandle;
            void begin();
            void submit_and_wait();
        };

        struct CircularBufferPosition
        {
            BufferH buffer_h;
            usize offset;
            usize length;
        };

        struct API
        {
            Context ctx;

            // todo: pool/arena data structure
            std::vector<Image> images;
            std::vector<RenderTarget> rendertargets;
            std::vector<Sampler> samplers;
            std::vector<Buffer> buffers;
            std::vector<FrameBuffer> framebuffers;
            std::vector<RenderPass> renderpasses;
            std::vector<Program> programs;
            std::vector<Shader> shaders;

            BufferH staging_buffer_h;
            usize   staging_buffer_offset;

            static API create(const Window& window);
            void destroy();

            void draw(); // TODO: used to make the HL API before the RenderGraph, remove once it's done

            void on_resize(int width, int height);
            void start_frame();
            void end_frame();
            void wait_idle();

            /// --- Drawing
            void begin_pass(const PassInfo&);
            void end_pass();


            /// ---

            CircularBufferPosition copy_to_staging_buffer(void* data, usize len);

            /// --- Resources
            ImageH create_image(const ImageInfo&);
            Image& get_image(ImageH);
            void destroy_image(ImageH);
            void upload_image(ImageH, void* data, usize len);

            RenderTargetH create_rendertarget(const RTInfo&);
            RenderTarget& get_rendertarget(RenderTargetH);

            BufferH create_buffer(const BufferInfo&);
            Buffer& get_buffer(BufferH);
            void destroy_buffer(BufferH);

            CommandBuffer get_temp_cmd_buffer();
        };
    }
}
