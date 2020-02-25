#pragma once

#include <vulkan/vulkan.hpp>
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
	    VmaMemoryUsage mem_usage;
	    vk::ImageView default_view;
	};
	using ImageH = Handle<Image>;

	struct Sampler
	{
	    vk::UniqueSampler vkhandle;
	};

	struct Buffer
	{
	    vk::UniqueBuffer vkhandle;
	};

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

	    /// --- Resources
	    ImageH create_image(const ImageInfo&);
	    Image& get_image(ImageH);
	    void destroy_image(ImageH);

	    RenderTargetH create_rendertarget(const RTInfo&);
	    RenderTarget& get_rendertarget(RenderTargetH);
	};
    }
}
