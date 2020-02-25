#include <vulkan/vulkan.hpp>
#include "renderer/hl_api.hpp"
#include "tools.hpp"
#include <iostream>
#include <algorithm>

namespace my_app::vulkan
{

    /// --- Render Target

    RenderTargetH API::create_rendertarget(const RTInfo& info)
    {
        RenderTarget rt;
        rt.is_swapchain = info.is_swapchain;

        rendertargets.push_back(std::move(rt));

        u32 h = static_cast<u32>(rendertargets.size()) - 1;
        return RenderTargetH(h);
    }

    RenderTarget& API::get_rendertarget(RenderTargetH H)
    {
        return rendertargets[H.value()];
    }

    /// --- Images

    static vk::ImageViewType view_type_from(vk::ImageType _type)
    {
        switch (_type)
        {
        case vk::ImageType::e1D: return vk::ImageViewType::e1D;
        case vk::ImageType::e2D: return vk::ImageViewType::e2D;
        case vk::ImageType::e3D: return vk::ImageViewType::e3D;
        }
        return vk::ImageViewType::e2D;
    }

    ImageH API::create_image(const ImageInfo& info)
    {
        Image img;

        img.name = info.name;
        img.memory_usage = VMA_MEMORY_USAGE_GPU_ONLY;

        img.image_info.imageType = info.type;
        img.image_info.format = info.format;
        img.image_info.extent.width = info.width;
        img.image_info.extent.height = info.height;
        img.image_info.extent.depth = info.depth;
        img.image_info.mipLevels = info.mip_levels;
        img.image_info.arrayLayers = info.layers;
        img.image_info.samples = info.samples;
        img.image_info.initialLayout = vk::ImageLayout::eUndefined;
        img.image_info.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
        img.image_info.queueFamilyIndexCount = 0;
        img.image_info.pQueueFamilyIndices = nullptr;
        img.image_info.sharingMode = vk::SharingMode::eExclusive;

        VmaAllocationCreateInfo alloc_info{};
        alloc_info.flags = VMA_ALLOCATION_CREATE_USER_DATA_COPY_STRING_BIT;
        alloc_info.usage = img.memory_usage;
        alloc_info.pUserData = const_cast<void*>(reinterpret_cast<const void*>(info.name));

        VK_CHECK(vmaCreateImage(ctx.allocator,
                                reinterpret_cast<VkImageCreateInfo*>(&img.image_info),
                                &alloc_info,
                                reinterpret_cast<VkImage*>(&img.vkhandle),
                                &img.allocation,
                                nullptr));

        ctx.device->setDebugUtilsObjectNameEXT(vk::DebugUtilsObjectNameInfoEXT{vk::ObjectType::eImage, get_raw_vulkan_handle(img.vkhandle), info.name});

        img.access = THSVS_ACCESS_NONE;

        img.full_range.aspectMask = vk::ImageAspectFlagBits::eColor;
        img.full_range.baseMipLevel = 0;
        img.full_range.levelCount = img.image_info.mipLevels;
        img.full_range.baseArrayLayer = 0;
        img.full_range.layerCount = img.image_info.arrayLayers;

        vk::ImageViewCreateInfo vci{};
        vci.flags = {};
        vci.image = img.vkhandle;
        vci.format = img.image_info.format;
        vci.components = { vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA };
        vci.subresourceRange = img.full_range;
        vci.viewType = view_type_from(img.image_info.imageType);

        if (img.image_info.usage & vk::ImageUsageFlagBits::eDepthStencilAttachment)
        {
            vci.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
        }

        img.default_view = ctx.device->createImageView(vci);


        images.push_back(std::move(img));
        return ImageH(static_cast<u32>(images.size()) - 1);
    }

    Image& API::get_image(ImageH H)
    {
        return images[H.value()];
    }

    static void destroy_image_internal(API& api, Image& img)
    {
        vmaDestroyImage(api.ctx.allocator, img.vkhandle, img.allocation);
        api.ctx.device->destroy(img.default_view);
    }

    void API::destroy_image(ImageH H)
    {
        Image& img = get_image(H);
        destroy_image_internal(*this, img);
    }

    static void transition_layout_internal(vk::CommandBuffer cmd, vk::Image image, ThsvsAccessType prev_access, ThsvsAccessType next_access, vk::ImageSubresourceRange subresource_range)
    {
        ThsvsImageBarrier image_barrier;
        image_barrier.prevAccessCount = 1;
        image_barrier.pPrevAccesses = &prev_access;
        image_barrier.nextAccessCount = 1;
        image_barrier.pNextAccesses = &next_access;
        image_barrier.prevLayout = THSVS_IMAGE_LAYOUT_OPTIMAL;
        image_barrier.nextLayout = THSVS_IMAGE_LAYOUT_OPTIMAL;
        image_barrier.discardContents = VK_FALSE;
        image_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        image_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        image_barrier.image = image;
        image_barrier.subresourceRange = subresource_range;

        thsvsCmdPipelineBarrier(
            cmd,
            nullptr,
            0,
            nullptr,
            1,
            &image_barrier);
    }


    void API::upload_image(ImageH H, void* data, usize len)
    {
        auto cmd_buffer = get_temp_cmd_buffer();

        const auto& staging = get_buffer(staging_buffer.buffer_h);
        auto staging_position = copy_to_staging_buffer(data, len);

        auto& image = get_image(H);
        const auto& range = image.full_range;

        cmd_buffer.begin();

        std::vector<vk::BufferImageCopy> copies;
        copies.reserve(range.levelCount);

        transition_layout_internal(*cmd_buffer.vkhandle, image.vkhandle, THSVS_ACCESS_NONE, THSVS_ACCESS_TRANSFER_WRITE, range);

        for (u32 i = range.baseMipLevel; i < range.baseMipLevel + range.levelCount; i++)
        {
            vk::BufferImageCopy copy;
            copy.bufferOffset = staging_position.offset;
            copy.imageSubresource.aspectMask = range.aspectMask;
            copy.imageSubresource.mipLevel = i;
            copy.imageSubresource.baseArrayLayer = range.baseArrayLayer;
            copy.imageSubresource.layerCount = range.layerCount;
            copy.imageExtent = image.image_info.extent;
            copies.push_back(std::move(copy));
        }

        cmd_buffer.vkhandle->copyBufferToImage(staging.vkhandle, image.vkhandle, vk::ImageLayout::eTransferDstOptimal, copies);

        image.access = THSVS_ACCESS_ANY_SHADER_READ_SAMPLED_IMAGE_OR_UNIFORM_TEXEL_BUFFER;

        transition_layout_internal(*cmd_buffer.vkhandle, image.vkhandle, THSVS_ACCESS_TRANSFER_WRITE, image.access, range);

        cmd_buffer.submit_and_wait();
    }

    /// --- Buffers

    BufferH API::create_buffer(const BufferInfo& info)
    {
        Buffer buf;

        buf.name = info.name;
        buf.memory_usage = info.memory_usage;
        buf.usage = info.usage;
        buf.mapped = nullptr;
        buf.size = info.size;

        vk::BufferCreateInfo ci{};
        ci.usage = info.usage;
        ci.size = info.size;

        VmaAllocationCreateInfo alloc_info{};
        alloc_info.usage = info.memory_usage;
        alloc_info.flags = VMA_ALLOCATION_CREATE_USER_DATA_COPY_STRING_BIT;
        alloc_info.pUserData = const_cast<void*>(reinterpret_cast<const void*>(info.name));

        VK_CHECK(vmaCreateBuffer(ctx.allocator,
                                 reinterpret_cast<VkBufferCreateInfo*>(&ci),
                                 &alloc_info,
                                 reinterpret_cast<VkBuffer*>(&buf.vkhandle),
                                 &buf.allocation,
                                 nullptr));

        ctx.device->setDebugUtilsObjectNameEXT(vk::DebugUtilsObjectNameInfoEXT{vk::ObjectType::eBuffer, get_raw_vulkan_handle(buf.vkhandle), info.name});

        buffers.push_back(std::move(buf));
        return BufferH(static_cast<u32>(buffers.size()) - 1);
    }

    Buffer& API::get_buffer(BufferH H)
    {
        return buffers[H.value()];
    }

    static void destroy_buffer_internal(API& api, Buffer& buf)
    {
        if (buf.mapped) {
            vmaUnmapMemory(api.ctx.allocator, buf.allocation);
        }
        vmaDestroyBuffer(api.ctx.allocator, buf.vkhandle, buf.allocation);
    }

    static void* buffer_map_internal(API& api, Buffer& buf)
    {
        if (buf.mapped == nullptr) {
            vmaMapMemory(api.ctx.allocator, buf.allocation, &buf.mapped);
        }
        return buf.mapped;
    }

    void API::destroy_buffer(BufferH H)
    {
        Buffer& buf = get_buffer(H);
        destroy_buffer_internal(*this, buf);
    }

    /// --- Command buffer

    CommandBuffer API::get_temp_cmd_buffer()
    {
        CommandBuffer cmd{ctx};
        auto& frame_resource = ctx.frame_resources.get_current();

        cmd.vkhandle = std::move(ctx.device->allocateCommandBuffersUnique({ *frame_resource.command_pool, vk::CommandBufferLevel::ePrimary, 1 })[0]);

        return cmd;
    }

    void CommandBuffer::begin()
    {
        vkhandle->begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
    }

    void CommandBuffer::submit_and_wait()
    {
        vk::UniqueFence fence = ctx.device->createFenceUnique({});
        auto graphics_queue = ctx.device->getQueue(ctx.graphics_family_idx, 0);

        vkhandle->end();

        vk::SubmitInfo si{};
        si.commandBufferCount = 1;
        si.pCommandBuffers = &vkhandle.get();
        graphics_queue.submit(si, *fence);

        ctx.device->waitForFences({ *fence }, VK_FALSE, UINT64_MAX);
    }

    /// --- Circular buffers

    static CircularBufferPosition map_circular_buffer_internal(API& api, CircularBuffer& circular, usize len)
    {
        Buffer& buffer = api.get_buffer(circular.buffer_h);
        usize& current_offset = circular.offset;

        if (current_offset + len > buffer.size) {
            current_offset = 0;
        }

        buffer_map_internal(api, buffer);

        CircularBufferPosition pos;
        pos.buffer_h = circular.buffer_h;
        pos.offset = current_offset;
        pos.length = len;
        pos.mapped = ptr_offset(buffer.mapped, current_offset);

        return pos;
    }

    static CircularBufferPosition copy_circular_buffer_internal(API& api, CircularBuffer& circular, void* data, usize len)
    {
        CircularBufferPosition pos = map_circular_buffer_internal(api, circular, len);
        std::memcpy(pos.mapped, data, len);
        pos.mapped = nullptr;
        return pos;
    }

    CircularBufferPosition API::copy_to_staging_buffer(void* data, usize len)
    {
        return copy_circular_buffer_internal(*this, staging_buffer, data, len);
    }

    CircularBufferPosition API::dynamic_vertex_buffer(usize len)
    {
        return map_circular_buffer_internal(*this, vertex_buffer, len);
    }

    CircularBufferPosition API::dynamic_index_buffer(usize len)
    {
        return map_circular_buffer_internal(*this, index_buffer, len);
    }

    /// --- Shaders

    ShaderH API::create_shader(const char* path)
    {
        Shader shader;

        auto code = tools::readFile(path);
        // keep code for reflection?

        vk::ShaderModuleCreateInfo info{};
        info.codeSize = code.size();
        info.pCode = reinterpret_cast<const u32*>(code.data());
        shader.vkhandle = ctx.device->createShaderModuleUnique(info);

        shaders.push_back(std::move(shader));
        return ShaderH(static_cast<u32>(shaders.size()) - 1);
    }

    Shader& API::get_shader(ShaderH H)
    {
        return shaders[H.value()];
    }

    void API::destroy_shader(ShaderH)
    {
    }

    /// --- Programs

    void ProgramInfo::push_constant(PushConstantInfo&& push_constant)
    {
        push_constants.push_back(std::move(push_constant));
    }

    void ProgramInfo::binding(BindingInfo&& binding)
    {
        bindings.push_back(std::move(binding));
    }

    ProgramH API::create_program(ProgramInfo&& info)
    {
        Program program;

        /// --- Create descriptor set layout

        std::vector<vk::DescriptorSetLayoutBinding> bindings;
        bindings.reserve(info.bindings.size());

        std::transform(info.bindings.begin(), info.bindings.end(), std::back_inserter(bindings),
                       [](const auto& info_binding) {
                           vk::DescriptorSetLayoutBinding binding;
                           binding.binding = info_binding.slot;
                           binding.stageFlags = info_binding.stages;
                           binding.descriptorType = info_binding.type;
                           binding.descriptorCount = info_binding.count;
                           return binding;
                       }
            );

        vk::DescriptorSetLayoutCreateInfo dslci{};
        dslci.bindingCount = static_cast<u32>(bindings.size());
        dslci.pBindings = bindings.data();
        program.descriptor_layout = ctx.device->createDescriptorSetLayoutUnique(dslci);

        /// --- Create pipeline layout

        std::vector<vk::PushConstantRange> pc_ranges;
        pc_ranges.reserve(info.push_constants.size());

        std::transform(info.push_constants.begin(), info.push_constants.end(), std::back_inserter(pc_ranges),
                       [](const auto& push_constant) {
                           vk::PushConstantRange range;
                           range.stageFlags = push_constant.stages;
                           range.offset = push_constant.offset;
                           range.size = push_constant.size;
                           return range;
                       }
            );

        vk::PipelineLayoutCreateInfo ci{};
        ci.pSetLayouts = &program.descriptor_layout.get();
        ci.setLayoutCount = 1;
        ci.pushConstantRangeCount = static_cast<u32>(pc_ranges.size());
        ci.pPushConstantRanges = pc_ranges.data();

        program.pipeline_layout = ctx.device->createPipelineLayoutUnique(ci);
        program.info = std::move(info);

        programs.push_back(std::move(program));
        return ProgramH(static_cast<u32>(programs.size()) - 1);
    }

    Program& API::get_program(ProgramH H)
    {
        return programs[H.value()];
    }

    void API::destroy_program(ProgramH)
    {

    }
}
