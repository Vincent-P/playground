#include "renderer/hl_api.hpp"
#include "tools.hpp"
#include <algorithm>
#include <iostream>
#include <vulkan/vulkan.hpp>
#include <cassert>
#include "types.hpp"

namespace my_app::vulkan
{

/// --- Render Target

RenderTargetH API::create_rendertarget(const RTInfo &info)
{
    RenderTarget rt;
    rt.is_swapchain = info.is_swapchain;
    rt.image_h = info.image_h;

    rendertargets.push_back(std::move(rt));

    u32 h = static_cast<u32>(rendertargets.size()) - 1;
    return RenderTargetH(h);
}

RenderTarget &API::get_rendertarget(RenderTargetH H)
{
    assert(H.is_valid());
    return rendertargets[H.value()];
}

/// --- Images

static vk::ImageViewType view_type_from(vk::ImageType _type)
{
    switch (_type) {
    case vk::ImageType::e1D:
        return vk::ImageViewType::e1D;
    case vk::ImageType::e2D:
        return vk::ImageViewType::e2D;
    case vk::ImageType::e3D:
        return vk::ImageViewType::e3D;
    }
    return vk::ImageViewType::e2D;
}

ImageH API::create_image(const ImageInfo &info)
{
    Image img;

    img.name         = info.name;
    img.memory_usage = VMA_MEMORY_USAGE_GPU_ONLY;

    img.image_info.imageType             = info.type;
    img.image_info.format                = info.format;
    img.image_info.extent.width          = info.width;
    img.image_info.extent.height         = info.height;
    img.image_info.extent.depth          = info.depth;
    img.image_info.mipLevels             = 1;
    img.image_info.arrayLayers           = info.layers;
    img.image_info.samples               = info.samples;
    img.image_info.initialLayout         = vk::ImageLayout::eUndefined;
    img.image_info.usage                 = info.usages;
    img.image_info.queueFamilyIndexCount = 0;
    img.image_info.pQueueFamilyIndices   = nullptr;
    img.image_info.sharingMode           = vk::SharingMode::eExclusive;

    if (info.generate_mip_levels) {
        img.image_info.mipLevels = static_cast<u32>(std::floor(std::log2(std::max(info.width, info.height))) + 1.0);
        img.image_info.usage |= vk::ImageUsageFlagBits::eTransferSrc;
    }

    VmaAllocationCreateInfo alloc_info{};
    alloc_info.flags     = VMA_ALLOCATION_CREATE_USER_DATA_COPY_STRING_BIT;
    alloc_info.usage     = img.memory_usage;
    alloc_info.pUserData = const_cast<void *>(reinterpret_cast<const void *>(info.name));

    VK_CHECK(vmaCreateImage(ctx.allocator, reinterpret_cast<VkImageCreateInfo *>(&img.image_info), &alloc_info,
                            reinterpret_cast<VkImage *>(&img.vkhandle), &img.allocation, nullptr));

    if (ENABLE_VALIDATION_LAYERS) {
        ctx.device->setDebugUtilsObjectNameEXT(
            vk::DebugUtilsObjectNameInfoEXT{vk::ObjectType::eImage, get_raw_vulkan_handle(img.vkhandle), info.name});
    }

    img.access = THSVS_ACCESS_NONE;
    img.layout = vk::ImageLayout::eUndefined;

    img.full_range.aspectMask     = vk::ImageAspectFlagBits::eColor;
    img.full_range.baseMipLevel   = 0;
    img.full_range.levelCount     = img.image_info.mipLevels;
    img.full_range.baseArrayLayer = 0;
    img.full_range.layerCount     = img.image_info.arrayLayers;

    vk::ImageViewCreateInfo vci{};
    vci.flags  = {};
    vci.image  = img.vkhandle;
    vci.format = img.image_info.format;
    vci.components
        = {vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA};
    vci.subresourceRange = img.full_range;
    vci.viewType         = view_type_from(img.image_info.imageType);

    if (img.image_info.usage & vk::ImageUsageFlagBits::eDepthStencilAttachment) {
        vci.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
    }

    img.default_view = ctx.device->createImageView(vci);

    vk::SamplerCreateInfo sci{};
    sci.magFilter        = vk::Filter::eNearest;
    sci.minFilter        = vk::Filter::eNearest;
    sci.mipmapMode       = vk::SamplerMipmapMode::eLinear;
    sci.addressModeU     = vk::SamplerAddressMode::eRepeat;
    sci.addressModeV     = vk::SamplerAddressMode::eRepeat;
    sci.addressModeW     = vk::SamplerAddressMode::eRepeat;
    sci.compareOp        = vk::CompareOp::eNever;
    sci.borderColor      = vk::BorderColor::eFloatOpaqueWhite;
    sci.minLod           = 0;
    sci.maxLod           = 0;
    sci.maxAnisotropy    = 8.0f;
    sci.anisotropyEnable = VK_TRUE;
    img.default_sampler  = ctx.device->createSampler(sci);

    images.push_back(std::move(img));
    return ImageH(static_cast<u32>(images.size()) - 1);
}

Image &API::get_image(ImageH H)
{
    assert(H.is_valid());
    return images[H.value()];
}

static void destroy_image_internal(API &api, Image &img)
{
    vmaDestroyImage(api.ctx.allocator, img.vkhandle, img.allocation);
    api.ctx.device->destroy(img.default_view);
    api.ctx.device->destroy(img.default_sampler);
}

void API::destroy_image(ImageH H)
{
    Image &img = get_image(H);
    destroy_image_internal(*this, img);
}

static void transition_layout_internal(vk::CommandBuffer cmd, vk::Image image, ThsvsAccessType prev_access,
                                       ThsvsAccessType next_access, vk::ImageSubresourceRange subresource_range)
{
    ThsvsImageBarrier image_barrier;
    image_barrier.prevAccessCount     = 1;
    image_barrier.pPrevAccesses       = &prev_access;
    image_barrier.nextAccessCount     = 1;
    image_barrier.pNextAccesses       = &next_access;
    image_barrier.prevLayout          = THSVS_IMAGE_LAYOUT_OPTIMAL;
    image_barrier.nextLayout          = THSVS_IMAGE_LAYOUT_OPTIMAL;
    image_barrier.discardContents     = VK_FALSE;
    image_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_barrier.image               = image;
    image_barrier.subresourceRange    = subresource_range;

    thsvsCmdPipelineBarrier(cmd, nullptr, 0, nullptr, 1, &image_barrier);
}

void API::upload_image(ImageH H, void *data, usize len)
{
    auto cmd_buffer = get_temp_cmd_buffer();

    const auto &staging   = get_buffer(staging_buffer.buffer_h);
    auto staging_position = copy_to_staging_buffer(data, len);

    auto &image       = get_image(H);
    auto range = image.full_range;
    range.levelCount = 1; // TODO: mips?

    cmd_buffer.begin();

    std::vector<vk::BufferImageCopy> copies;
    copies.reserve(range.levelCount);
    transition_layout_internal(*cmd_buffer.vkhandle, image.vkhandle, THSVS_ACCESS_NONE, THSVS_ACCESS_TRANSFER_WRITE, range);

    for (u32 i = range.baseMipLevel; i < range.baseMipLevel + range.levelCount; i++) {
        vk::BufferImageCopy copy;
        copy.bufferOffset                    = staging_position.offset;
        copy.imageSubresource.aspectMask     = range.aspectMask;
        copy.imageSubresource.mipLevel       = i;
        copy.imageSubresource.baseArrayLayer = range.baseArrayLayer;
        copy.imageSubresource.layerCount     = range.layerCount;
        copy.imageExtent                     = image.image_info.extent;
        copies.push_back(std::move(copy));
    }

    cmd_buffer.vkhandle->copyBufferToImage(staging.vkhandle, image.vkhandle, vk::ImageLayout::eTransferDstOptimal,
                                           copies);

    image.access = THSVS_ACCESS_ANY_SHADER_READ_SAMPLED_IMAGE_OR_UNIFORM_TEXEL_BUFFER;
    transition_layout_internal(*cmd_buffer.vkhandle, image.vkhandle, THSVS_ACCESS_TRANSFER_WRITE, image.access, range);
    image.layout = vk::ImageLayout::eShaderReadOnlyOptimal;

    cmd_buffer.submit_and_wait();
}

void API::generate_mipmaps(ImageH h)
{
    auto cmd_buffer = get_temp_cmd_buffer();
    auto &image       = get_image(h);

    u32 width = image.image_info.extent.width;
    u32 height = image.image_info.extent.height;
    u32 mip_levels = image.image_info.mipLevels;

    if (mip_levels == 1) {
        return;
    }

    cmd_buffer.begin();


    auto& cmd = cmd_buffer.vkhandle;

    {
        vk::ImageSubresourceRange mip_sub_range(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
        vk::ImageMemoryBarrier b{};
        b.oldLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        b.newLayout = vk::ImageLayout::eTransferSrcOptimal;
        b.srcAccessMask = vk::AccessFlags();
        b.dstAccessMask = vk::AccessFlagBits::eTransferRead;
        b.image = image.vkhandle;
        b.subresourceRange = mip_sub_range;
        cmd->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {}, nullptr, nullptr, b);
    }

    for (u32 i = 1; i < mip_levels; i++)
    {
        auto src_width = width >> (i - 1);
        auto src_height = height >> (i - 1);
        auto dst_width = width >> i;
        auto dst_height = height >> i;

        vk::ImageBlit blit{};
        blit.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        blit.srcSubresource.layerCount = 1;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcOffsets[1].x = static_cast<i32>(src_width);
        blit.srcOffsets[1].y = static_cast<i32>(src_height);
        blit.srcOffsets[1].z = 1;
        blit.dstSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        blit.dstSubresource.layerCount = 1;
        blit.dstSubresource.mipLevel = i;
        blit.dstOffsets[1].x = static_cast<i32>(dst_width);
        blit.dstOffsets[1].y = static_cast<i32>(dst_height);
        blit.dstOffsets[1].z = 1;

        vk::ImageSubresourceRange mip_sub_range(vk::ImageAspectFlagBits::eColor, i, 1, 0, 1);

        {
            vk::ImageMemoryBarrier b{};
            b.oldLayout = vk::ImageLayout::eUndefined;
            b.newLayout = vk::ImageLayout::eTransferDstOptimal;
            b.srcAccessMask = vk::AccessFlags();
            b.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
            b.image = image.vkhandle;
            b.subresourceRange = mip_sub_range;
            cmd->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {}, nullptr, nullptr, b);
        }

        cmd->blitImage(image.vkhandle, vk::ImageLayout::eTransferSrcOptimal, image.vkhandle, vk::ImageLayout::eTransferDstOptimal, blit, vk::Filter::eLinear);

        {
            vk::ImageMemoryBarrier b{};
            b.oldLayout = vk::ImageLayout::eTransferDstOptimal;
            b.newLayout = vk::ImageLayout::eTransferSrcOptimal;
            b.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
            b.dstAccessMask = vk::AccessFlagBits::eTransferRead;
            b.image = image.vkhandle;
            b.subresourceRange = mip_sub_range;
            cmd->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {}, nullptr, nullptr, b);
        }
    }

    image.access = THSVS_ACCESS_ANY_SHADER_READ_SAMPLED_IMAGE_OR_UNIFORM_TEXEL_BUFFER;
    transition_layout_internal(*cmd_buffer.vkhandle, image.vkhandle, THSVS_ACCESS_TRANSFER_READ, image.access, image.full_range);
    image.layout = vk::ImageLayout::eShaderReadOnlyOptimal;

    cmd_buffer.submit_and_wait();
}

/// --- Samplers


SamplerH API::create_sampler(const SamplerInfo &info)
{
    Sampler sampler;

    vk::SamplerCreateInfo sci{};
    sci.magFilter        = info.mag_filter;
    sci.minFilter        = info.min_filter;
    sci.mipmapMode       = info.mip_map_mode;
    sci.addressModeU     = info.address_mode;
    sci.addressModeV     = info.address_mode;
    sci.addressModeW     = info.address_mode;
    sci.compareOp        = vk::CompareOp::eNever;
    sci.borderColor      = vk::BorderColor::eFloatOpaqueWhite;
    sci.minLod           = 0;
    sci.maxLod           = 0;
    sci.maxAnisotropy    = 8.0f;
    sci.anisotropyEnable = VK_TRUE;
    sampler.vkhandle     = ctx.device->createSamplerUnique(sci);
    sampler.info = info;

    samplers.push_back(std::move(sampler));
    return SamplerH(static_cast<u32>(samplers.size()) - 1);
}

Sampler &API::get_sampler(SamplerH H)
{
    assert(H.is_valid());
    return samplers[H.value()];
}

void API::destroy_sampler(SamplerH H)
{
}

/// --- Buffers

BufferH API::create_buffer(const BufferInfo &info)
{
    Buffer buf;

    buf.name         = info.name;
    buf.memory_usage = info.memory_usage;
    buf.usage        = info.usage;
    buf.mapped       = nullptr;
    buf.size         = info.size;

    vk::BufferCreateInfo ci{};
    ci.usage = info.usage;
    ci.size  = info.size;

    VmaAllocationCreateInfo alloc_info{};
    alloc_info.usage     = info.memory_usage;
    alloc_info.flags     = VMA_ALLOCATION_CREATE_USER_DATA_COPY_STRING_BIT;
    alloc_info.pUserData = const_cast<void *>(reinterpret_cast<const void *>(info.name));

    VK_CHECK(vmaCreateBuffer(ctx.allocator, reinterpret_cast<VkBufferCreateInfo *>(&ci), &alloc_info,
                             reinterpret_cast<VkBuffer *>(&buf.vkhandle), &buf.allocation, nullptr));

    if (ENABLE_VALIDATION_LAYERS) {
        ctx.device->setDebugUtilsObjectNameEXT(
            vk::DebugUtilsObjectNameInfoEXT{vk::ObjectType::eBuffer, get_raw_vulkan_handle(buf.vkhandle), info.name});
    }

    buffers.push_back(std::move(buf));
    return BufferH(static_cast<u32>(buffers.size()) - 1);
}

Buffer &API::get_buffer(BufferH H)
{
    assert(H.is_valid());
    return buffers[H.value()];
}

static void destroy_buffer_internal(API &api, Buffer &buf)
{
    if (buf.mapped) {
        vmaUnmapMemory(api.ctx.allocator, buf.allocation);
    }
    vmaDestroyBuffer(api.ctx.allocator, buf.vkhandle, buf.allocation);
}

static void *buffer_map_internal(API &api, Buffer &buf)
{
    if (buf.mapped == nullptr) {
        vmaMapMemory(api.ctx.allocator, buf.allocation, &buf.mapped);
    }
    return buf.mapped;
}

void API::destroy_buffer(BufferH H)
{
    Buffer &buf = get_buffer(H);
    destroy_buffer_internal(*this, buf);
}

void API::upload_buffer(BufferH H, void *data, usize len)
{
    auto cmd_buffer = get_temp_cmd_buffer();

    const auto &staging   = get_buffer(staging_buffer.buffer_h);
    auto staging_position = copy_to_staging_buffer(data, len);

    auto &buffer       = get_buffer(H);

    cmd_buffer.begin();

    vk::BufferCopy copy;
    copy.srcOffset = staging_position.offset;
    copy.dstOffset = 0;
    copy.size      = len;
    cmd_buffer.vkhandle->copyBuffer(staging.vkhandle, buffer.vkhandle, copy);

    cmd_buffer.submit_and_wait();
}

/// --- Command buffer

CommandBuffer API::get_temp_cmd_buffer()
{
    CommandBuffer cmd{ctx, {}};
    auto &frame_resource = ctx.frame_resources.get_current();

    cmd.vkhandle = std::move(ctx.device->allocateCommandBuffersUnique(
        {*frame_resource.command_pool, vk::CommandBufferLevel::ePrimary, 1})[0]);

    return cmd;
}

void CommandBuffer::begin() { vkhandle->begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit}); }

void CommandBuffer::submit_and_wait()
{
    vk::UniqueFence fence = ctx.device->createFenceUnique({});
    auto graphics_queue   = ctx.device->getQueue(ctx.graphics_family_idx, 0);

    vkhandle->end();

    vk::SubmitInfo si{};
    si.commandBufferCount = 1;
    si.pCommandBuffers    = &vkhandle.get();
    graphics_queue.submit(si, *fence);

    ctx.device->waitForFences({*fence}, VK_FALSE, UINT64_MAX);
}

/// --- Circular buffers

CircularBufferPosition map_circular_buffer_internal(API &api, CircularBuffer &circular, usize len)
{
    Buffer &buffer        = api.get_buffer(circular.buffer_h);
    usize &current_offset = circular.offset;

    constexpr uint min_uniform_buffer_alignment = 256u;
    len                                         = round_up_to_alignment(min_uniform_buffer_alignment, len);

    if (current_offset + len > buffer.size) {
        current_offset = 0;
    }

    buffer_map_internal(api, buffer);

    CircularBufferPosition pos;
    pos.buffer_h = circular.buffer_h;
    pos.offset   = current_offset;
    pos.length   = len;
    pos.mapped   = ptr_offset(buffer.mapped, current_offset);

    current_offset += len;

    return pos;
}

static CircularBufferPosition copy_circular_buffer_internal(API &api, CircularBuffer &circular, void *data, usize len)
{
    CircularBufferPosition pos = map_circular_buffer_internal(api, circular, len);
    std::memcpy(pos.mapped, data, len);
    pos.mapped = nullptr;
    return pos;
}

CircularBufferPosition API::copy_to_staging_buffer(void *data, usize len)
{
    return copy_circular_buffer_internal(*this, staging_buffer, data, len);
}

CircularBufferPosition API::dynamic_vertex_buffer(usize len)
{
    return map_circular_buffer_internal(*this, dyn_vertex_buffer, len);
}

CircularBufferPosition API::dynamic_uniform_buffer(usize len)
{
    return map_circular_buffer_internal(*this, dyn_uniform_buffer, len);
}

CircularBufferPosition API::dynamic_index_buffer(usize len)
{
    return map_circular_buffer_internal(*this, dyn_index_buffer, len);
}

/// --- Shaders

ShaderH API::create_shader(const char *path)
{
    Shader shader;

    shader.name = path;
    auto code   = tools::read_file(path);
    // keep code for reflection?

    vk::ShaderModuleCreateInfo info{};
    info.codeSize   = code.size();
    info.pCode      = reinterpret_cast<const u32 *>(code.data());
    shader.vkhandle = ctx.device->createShaderModuleUnique(info);

    shaders.push_back(std::move(shader));
    return ShaderH(static_cast<u32>(shaders.size()) - 1);
}

Shader &API::get_shader(ShaderH H)
{
    assert(H.is_valid());
    return shaders[H.value()];
}

void API::destroy_shader(ShaderH /*unused*/) {}

/// --- Programs

void ProgramInfo::push_constant(PushConstantInfo &&push_constant)
{
    push_constants.push_back(std::move(push_constant));
}

void ProgramInfo::binding(BindingInfo &&binding) { bindings_by_set[binding.set].push_back(std::move(binding)); }

void ProgramInfo::vertex_stride(u32 value) { vertex_buffer_info.stride = value; }

void ProgramInfo::vertex_info(VertexInfo &&info) { vertex_buffer_info.vertices_info.push_back(std::move(info)); }

ProgramH API::create_program(ProgramInfo &&info)
{
    Program program;

    /// --- Create descriptor set layout

    for (uint i = 0; i < MAX_DESCRIPTOR_SET; i++)
    {
        std::vector<vk::DescriptorSetLayoutBinding> bindings;
        program.dynamic_count_by_set[i] = 0;

        map_transform(info.bindings_by_set[i], bindings, [&](const auto &info_binding) {
            vk::DescriptorSetLayoutBinding binding;
            binding.binding         = info_binding.slot;
            binding.stageFlags      = info_binding.stages;
            binding.descriptorType  = info_binding.type;

            if (binding.descriptorType == vk::DescriptorType::eUniformBufferDynamic) {
                program.dynamic_count_by_set[i]++;
            }

            binding.descriptorCount = info_binding.count;
            return binding;
        });

        vk::StructureChain<vk::DescriptorSetLayoutCreateInfo, vk::DescriptorSetLayoutBindingFlagsCreateInfo> create_info;
        // clang-format off
        std::vector<vk::DescriptorBindingFlags> flags{info.bindings_by_set[i].size(), vk::DescriptorBindingFlags{/*vk::DescriptorBindingFlagBits::eUpdateAfterBind*/}};
        // clang-format on
        auto &flags_info         = create_info.get<vk::DescriptorSetLayoutBindingFlagsCreateInfo>();
        flags_info.bindingCount  = static_cast<u32>(bindings.size());
        flags_info.pBindingFlags = flags.data();

        auto &layout_info        = create_info.get<vk::DescriptorSetLayoutCreateInfo>();
        layout_info.flags        = {/*vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool*/};
        layout_info.bindingCount = static_cast<u32>(bindings.size());
        layout_info.pBindings    = bindings.data();

        program.descriptor_layouts[i] = ctx.device->createDescriptorSetLayoutUnique(layout_info);
    }

    /// --- Create pipeline layout

    std::vector<vk::PushConstantRange> pc_ranges;
    map_transform(info.push_constants, pc_ranges, [](const auto &push_constant) {
        vk::PushConstantRange range;
        range.stageFlags = push_constant.stages;
        range.offset     = push_constant.offset;
        range.size       = push_constant.size;
        return range;
    });

    std::vector<vk::DescriptorSetLayout> layouts;

    for (const auto& unique_layout: program.descriptor_layouts) {
        if (unique_layout) {
            layouts.push_back(*unique_layout);
        }
    }


    vk::PipelineLayoutCreateInfo ci{};
    ci.pSetLayouts            = layouts.data();
    ci.setLayoutCount         = static_cast<u32>(layouts.size());
    ci.pPushConstantRanges    = pc_ranges.data();
    ci.pushConstantRangeCount = static_cast<u32>(pc_ranges.size());

    program.pipeline_layout = ctx.device->createPipelineLayoutUnique(ci);
    program.info            = std::move(info);

    for (uint i = 0; i < MAX_DESCRIPTOR_SET; i++) {
        program.data_dirty_by_set[i] = true;
    }


    programs.push_back(std::move(program));
    return ProgramH(static_cast<u32>(programs.size()) - 1);
}

Program &API::get_program(ProgramH H)
{
    assert(H.is_valid());
    return programs[H.value()];
}

void API::destroy_program(ProgramH /*unused*/) {}

} // namespace my_app::vulkan
