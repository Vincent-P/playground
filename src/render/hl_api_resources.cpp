#include "base/types.hpp"
#include "render/hl_api.hpp"
#include "tools.hpp"

#include <SPIRV-Reflect/spirv_reflect.h>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <fmt/core.h>
#include <iterator>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

namespace my_app::vulkan
{

static ImageViewH create_image_view(API &api, ImageH image_h, const Image &image, const VkImageSubresourceRange &range,
                                    VkFormat format);
static void destroy_image_view(API &api, ImageViewH H);

/// --- Images

static VkImageViewType view_type_from(VkImageType _type)
{
    switch (_type)
    {
        case VK_IMAGE_TYPE_1D:
            return VK_IMAGE_VIEW_TYPE_1D;
        case VK_IMAGE_TYPE_2D:
            return VK_IMAGE_VIEW_TYPE_2D;
        case VK_IMAGE_TYPE_3D:
            return VK_IMAGE_VIEW_TYPE_3D;
        case VK_IMAGE_TYPE_MAX_ENUM:
            break;
    }
    return VK_IMAGE_VIEW_TYPE_2D;
}

static ImageH create_image_internal(vulkan::API &api, const ImageInfo &info, VkImage external = VK_NULL_HANDLE)
{
    auto &ctx = api.ctx;

    ImageH image_h = api.images.add({});
    Image &img     = *api.images.get(image_h);

    img.name     = info.name;
    img.info     = info;
    img.is_proxy = external != VK_NULL_HANDLE;

    img.extra_formats = info.extra_formats;

    assert(info.mip_levels == 1 || !info.generate_mip_levels);

    VkImageCreateInfo image_info = {.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};

    if (!info.extra_formats.empty())
    {
        image_info.flags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
    }

    if (info.type == VK_IMAGE_TYPE_3D)
    {
        image_info.flags |= VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT;
    }

    image_info.imageType             = info.type;
    image_info.format                = info.format;
    image_info.extent.width          = info.width;
    image_info.extent.height         = info.height;
    image_info.extent.depth          = info.depth;
    image_info.mipLevels             = info.mip_levels;
    image_info.arrayLayers           = info.layers;
    image_info.samples               = info.samples;
    image_info.initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.usage                 = info.usages;
    image_info.queueFamilyIndexCount = 0;
    image_info.pQueueFamilyIndices   = nullptr;
    image_info.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
    image_info.tiling                = VK_IMAGE_TILING_OPTIMAL;

    if (info.generate_mip_levels)
    {
        image_info.mipLevels = static_cast<u32>(std::floor(std::log2(std::max(info.width, info.height))) + 1.0);
        img.info.mip_levels  = image_info.mipLevels;
        image_info.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }

    // Create the VkImage handle
    if (img.is_proxy)
    {
        img.vkhandle = external;
    }
    else
    {
        VmaAllocationCreateInfo alloc_info{};
        alloc_info.flags     = VMA_ALLOCATION_CREATE_USER_DATA_COPY_STRING_BIT;
        alloc_info.usage     = img.info.memory_usage;
        alloc_info.pUserData = const_cast<void *>(reinterpret_cast<const void *>(info.name));

        VK_CHECK(vmaCreateImage(ctx.allocator,
                                reinterpret_cast<VkImageCreateInfo *>(&image_info),
                                &alloc_info,
                                reinterpret_cast<VkImage *>(&img.vkhandle),
                                &img.allocation,
                                nullptr));
    }

    if (ENABLE_VALIDATION_LAYERS)
    {
        VkDebugUtilsObjectNameInfoEXT ni = {.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
        ni.objectHandle                  = reinterpret_cast<u64>(img.vkhandle);
        ni.objectType                    = VK_OBJECT_TYPE_IMAGE;
        ni.pObjectName                   = info.name;
        VK_CHECK(ctx.vkSetDebugUtilsObjectNameEXT(ctx.device, &ni));
    }

    img.usage = ImageUsage::None;

    img.full_range.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    img.full_range.baseMipLevel   = 0;
    img.full_range.levelCount     = image_info.mipLevels;
    img.full_range.baseArrayLayer = 0;
    img.full_range.layerCount     = image_info.arrayLayers;

    if (image_info.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
    {
        img.full_range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    /// --- Create views

    img.default_view = create_image_view(api, image_h, img, img.full_range, img.info.format);

    img.format_views.reserve(info.extra_formats.size());
    for (const auto &extra_format : info.extra_formats)
    {
        img.format_views.push_back(create_image_view(api, image_h, img, img.full_range, extra_format));
    }

    for (u32 i = 0; i < image_info.mipLevels; i++)
    {
        auto mip_range         = img.full_range;
        mip_range.baseMipLevel = i;
        mip_range.levelCount   = 1;
        img.mip_views.push_back(create_image_view(api, image_h, img, mip_range, img.info.format));
    }

    return image_h;
}

ImageH API::create_image(const ImageInfo &info) { return create_image_internal(*this, info); }

ImageH API::create_image_proxy(VkImage external, const ImageInfo &info)
{
    return create_image_internal(*this, info, external);
}

Image &API::get_image(ImageH H)
{
    assert(H.is_valid());
    return *images.get(H);
}

void destroy_image_internal(API &api, Image &img)
{
    if (img.is_proxy) {}
    else
    {
        vmaDestroyImage(api.ctx.allocator, img.vkhandle, img.allocation);
    }

    destroy_image_view(api, img.default_view);
    for (auto &image_view : img.format_views)
    {
        destroy_image_view(api, image_view);
    }
    for (auto &image_view : img.mip_views)
    {
        destroy_image_view(api, image_view);
    }

    img.format_views.clear();
    img.mip_views.clear();
}

void API::destroy_image(ImageH H)
{
    Image &img = get_image(H);
    destroy_image_internal(*this, img);
    images.remove(H);
}

static ImageViewH create_image_view(API &api, ImageH image_h, const Image &image, const VkImageSubresourceRange &range,
                                    VkFormat format)
{
    auto &ctx = api.ctx;

    ImageViewH view_h = api.image_views.add({});
    ImageView &view   = *api.image_views.get(view_h);

    view.image_h   = image_h;
    view.range     = range;
    view.format    = format;
    view.view_type = view_type_from(image.info.type);

    VkImageViewCreateInfo vci = {.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vci.flags                 = 0;
    vci.image                 = image.vkhandle;
    vci.format                = view.format;
    vci.components.r          = VK_COMPONENT_SWIZZLE_IDENTITY;
    vci.components.g          = VK_COMPONENT_SWIZZLE_IDENTITY;
    vci.components.b          = VK_COMPONENT_SWIZZLE_IDENTITY;
    vci.components.a          = VK_COMPONENT_SWIZZLE_IDENTITY;
    vci.subresourceRange      = view.range;
    vci.viewType              = view.view_type;

    VK_CHECK(vkCreateImageView(ctx.device, &vci, nullptr, &view.vkhandle));

    if (ENABLE_VALIDATION_LAYERS)
    {
        VkDebugUtilsObjectNameInfoEXT ni = {.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
        ni.objectHandle                  = reinterpret_cast<u64>(view.vkhandle);
        ni.objectType                    = VK_OBJECT_TYPE_IMAGE_VIEW;
        ni.pObjectName                   = image.info.name;
        VK_CHECK(ctx.vkSetDebugUtilsObjectNameEXT(ctx.device, &ni));
    }

    return view_h;
}

static void destroy_image_view(API &api, ImageViewH H)
{
    assert(H.is_valid());
    ImageView &view = *api.image_views.get(H);
    vkDestroyImageView(api.ctx.device, view.vkhandle, nullptr);
    api.image_views.remove(H);
}

ImageView &API::get_image_view(ImageViewH H)
{
    assert(H.is_valid());
    return *image_views.get(H);
}

void API::upload_image(ImageH H, void *data, usize len)
{
    auto cmd_buffer = get_temp_cmd_buffer();

    const auto &staging   = get_buffer(staging_buffer.buffer_h);
    auto staging_position = copy_to_staging_buffer(data, len);

    auto &image      = get_image(H);
    auto range       = image.full_range;
    range.levelCount = 1; // TODO: mips?

    cmd_buffer.begin();

    std::vector<VkBufferImageCopy> copies;
    copies.reserve(range.levelCount);

    {
        auto src = get_src_image_access(image.usage);
        auto dst = get_dst_image_access(ImageUsage::TransferDst);

        VkImageMemoryBarrier b = {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        b.oldLayout            = src.layout;
        b.newLayout            = dst.layout;
        b.srcAccessMask        = src.access;
        b.dstAccessMask        = dst.access;
        b.image                = image.vkhandle;
        b.subresourceRange     = range;
        vkCmdPipelineBarrier(cmd_buffer.vkhandle, src.stage, dst.stage, 0, 0, nullptr, 0, nullptr, 1, &b);
    }

    for (u32 i = range.baseMipLevel; i < range.baseMipLevel + range.levelCount; i++)
    {
        VkBufferImageCopy copy{};
        copy.bufferOffset                    = staging_position.offset;
        copy.imageSubresource.aspectMask     = range.aspectMask;
        copy.imageSubresource.mipLevel       = i;
        copy.imageSubresource.baseArrayLayer = range.baseArrayLayer;
        copy.imageSubresource.layerCount     = range.layerCount;
        copy.imageExtent                     = {image.info.width, image.info.height, image.info.depth};
        copies.push_back(std::move(copy));
    }

    vkCmdCopyBufferToImage(cmd_buffer.vkhandle,
                           staging.vkhandle,
                           image.vkhandle,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           copies.size(),
                           copies.data());

    image.usage = ImageUsage::TransferDst;

    cmd_buffer.submit_and_wait();
}

void API::generate_mipmaps(ImageH h)
{
    auto cmd_buffer = get_temp_cmd_buffer();
    auto &image     = get_image(h);

    u32 width      = image.info.width;
    u32 height     = image.info.height;
    u32 mip_levels = image.info.mip_levels;

    if (mip_levels == 1)
    {
        return;
    }

    cmd_buffer.begin();

    VkCommandBuffer cmd = cmd_buffer.vkhandle;

    VkImageSubresourceRange mip_sub_range{};
    mip_sub_range.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    mip_sub_range.baseArrayLayer = 0;
    mip_sub_range.layerCount     = 1;
    mip_sub_range.baseMipLevel   = 0;
    mip_sub_range.levelCount     = 1;

    {
        auto src = get_src_image_access(image.usage);
        auto dst = get_dst_image_access(ImageUsage::TransferSrc);

        VkImageMemoryBarrier b = {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        b.oldLayout            = src.layout;
        b.newLayout            = dst.layout;
        b.srcAccessMask        = src.access;
        b.dstAccessMask        = dst.access;
        b.image                = image.vkhandle;
        b.subresourceRange     = mip_sub_range;
        vkCmdPipelineBarrier(cmd, src.stage, dst.stage, 0, 0, nullptr, 0, nullptr, 1, &b);
    }

    for (u32 i = 1; i < mip_levels; i++)
    {
        auto src_width  = width >> (i - 1);
        auto src_height = height >> (i - 1);
        auto dst_width  = width >> i;
        auto dst_height = height >> i;

        VkImageBlit blit{};
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.layerCount = 1;
        blit.srcSubresource.mipLevel   = i - 1;
        blit.srcOffsets[1].x           = static_cast<i32>(src_width);
        blit.srcOffsets[1].y           = static_cast<i32>(src_height);
        blit.srcOffsets[1].z           = 1;
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.layerCount = 1;
        blit.dstSubresource.mipLevel   = i;
        blit.dstOffsets[1].x           = static_cast<i32>(dst_width);
        blit.dstOffsets[1].y           = static_cast<i32>(dst_height);
        blit.dstOffsets[1].z           = 1;

        mip_sub_range.baseMipLevel = i;

        {
            auto src = get_src_image_access(ImageUsage::None);
            auto dst = get_dst_image_access(ImageUsage::TransferDst);

            VkImageMemoryBarrier b = {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            b.oldLayout            = src.layout;
            b.newLayout            = dst.layout;
            b.srcAccessMask        = src.access;
            b.dstAccessMask        = dst.access;
            b.image                = image.vkhandle;
            b.subresourceRange     = mip_sub_range;
            vkCmdPipelineBarrier(cmd, src.stage, dst.stage, 0, 0, nullptr, 0, nullptr, 1, &b);
        }

        vkCmdBlitImage(cmd,
                       image.vkhandle,
                       VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       image.vkhandle,
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       1,
                       &blit,
                       VK_FILTER_LINEAR);

        {
            auto src = get_src_image_access(ImageUsage::TransferDst);
            auto dst = get_dst_image_access(ImageUsage::TransferSrc);

            VkImageMemoryBarrier b = {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            b.oldLayout            = src.layout;
            b.newLayout            = dst.layout;
            b.srcAccessMask        = src.access;
            b.dstAccessMask        = dst.access;
            b.image                = image.vkhandle;
            b.subresourceRange     = mip_sub_range;
            vkCmdPipelineBarrier(cmd, src.stage, dst.stage, 0, 0, nullptr, 0, nullptr, 1, &b);
        }
    }

    image.usage = ImageUsage::TransferSrc;

    cmd_buffer.submit_and_wait();
}

void API::transfer_done(ImageH H) // it's a hack for now
{
    auto cmd_buffer = get_temp_cmd_buffer();
    auto &image     = get_image(H);

    cmd_buffer.begin();

    auto src = get_src_image_access(image.usage);
    auto dst = get_dst_image_access(ImageUsage::GraphicsShaderRead);
    auto b   = get_image_barrier(image, src, dst);
    vkCmdPipelineBarrier(cmd_buffer.vkhandle, src.stage, dst.stage, 0, 0, nullptr, 0, nullptr, 1, &b);
    image.usage = ImageUsage::GraphicsShaderRead;
    cmd_buffer.submit_and_wait();
}

/// --- Samplers

SamplerH API::create_sampler(const SamplerInfo &info)
{
    Sampler sampler;

    VkSamplerCreateInfo sci = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sci.magFilter           = info.mag_filter;
    sci.minFilter           = info.min_filter;
    sci.mipmapMode          = info.mip_map_mode;
    sci.addressModeU        = info.address_mode;
    sci.addressModeV        = info.address_mode;
    sci.addressModeW        = info.address_mode;
    sci.compareOp           = VK_COMPARE_OP_NEVER;
    sci.borderColor         = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    sci.minLod              = 0;
    sci.maxLod              = 7;
    sci.maxAnisotropy       = 8.0f;
    sci.anisotropyEnable    = true;

    VK_CHECK(vkCreateSampler(ctx.device, &sci, nullptr, &sampler.vkhandle));
    sampler.info = info;

    return samplers.add(std::move(sampler));
}

Sampler &API::get_sampler(SamplerH H)
{
    assert(H.is_valid());
    return *samplers.get(H);
}

void destroy_sampler_internal(API &api, Sampler &sampler)
{
    vkDestroySampler(api.ctx.device, sampler.vkhandle, nullptr);
}

void API::destroy_sampler(SamplerH H)
{
    assert(H.is_valid());
    auto *sampler = samplers.get(H);
    destroy_sampler_internal(*this, *sampler);
    samplers.remove(H);
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

    VkBufferCreateInfo ci = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    ci.usage              = info.usage;
    ci.size               = info.size;

    VmaAllocationCreateInfo alloc_info{};
    alloc_info.usage     = info.memory_usage;
    alloc_info.flags     = VMA_ALLOCATION_CREATE_USER_DATA_COPY_STRING_BIT;
    alloc_info.pUserData = const_cast<void *>(reinterpret_cast<const void *>(info.name));

    VK_CHECK(vmaCreateBuffer(ctx.allocator,
                             reinterpret_cast<VkBufferCreateInfo *>(&ci),
                             &alloc_info,
                             reinterpret_cast<VkBuffer *>(&buf.vkhandle),
                             &buf.allocation,
                             nullptr));

    if (ENABLE_VALIDATION_LAYERS)
    {
        VkDebugUtilsObjectNameInfoEXT ni = {.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
        ni.objectHandle                  = reinterpret_cast<u64>(buf.vkhandle);
        ni.objectType                    = VK_OBJECT_TYPE_BUFFER;
        ni.pObjectName                   = info.name;
        VK_CHECK(ctx.vkSetDebugUtilsObjectNameEXT(ctx.device, &ni));
    }

    return buffers.add(std::move(buf));
}

Buffer &API::get_buffer(BufferH H)
{
    assert(H.is_valid());
    return *buffers.get(H);
}

void destroy_buffer_internal(API &api, Buffer &buf)
{
    if (buf.mapped)
    {
        vmaUnmapMemory(api.ctx.allocator, buf.allocation);
    }
    vmaDestroyBuffer(api.ctx.allocator, buf.vkhandle, buf.allocation);
}

static void *buffer_map_internal(API &api, Buffer &buf)
{
    if (buf.mapped == nullptr)
    {
        vmaMapMemory(api.ctx.allocator, buf.allocation, &buf.mapped);
    }
    return buf.mapped;
}

void API::destroy_buffer(BufferH H)
{
    assert(H.is_valid());
    Buffer &buf = get_buffer(H);
    destroy_buffer_internal(*this, buf);
    buffers.remove(H);
}

void API::upload_buffer(BufferH H, void *data, usize len)
{
    auto cmd_buffer = get_temp_cmd_buffer();

    const auto &staging   = get_buffer(staging_buffer.buffer_h);
    auto staging_position = copy_to_staging_buffer(data, len);

    auto &buffer = get_buffer(H);

    cmd_buffer.begin();

    VkBufferCopy copy;
    copy.srcOffset = staging_position.offset;
    copy.dstOffset = 0;
    copy.size      = len;
    vkCmdCopyBuffer(cmd_buffer.vkhandle, staging.vkhandle, buffer.vkhandle, 1, &copy);

    cmd_buffer.submit_and_wait();
}

/// --- Command buffer

CommandBuffer API::get_temp_cmd_buffer()
{
    CommandBuffer cmd{ctx, {}};
    auto &frame_resource = ctx.frame_resources.get_current();

    VkCommandBufferAllocateInfo ai = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ai.commandPool                 = frame_resource.command_pool;
    ai.level                       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount          = 1;
    VK_CHECK(vkAllocateCommandBuffers(ctx.device, &ai, &cmd.vkhandle));

    return cmd;
}

void CommandBuffer::begin() const
{
    VkCommandBufferBeginInfo binfo = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    binfo.flags                    = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(vkhandle, &binfo);
}

void CommandBuffer::submit_and_wait()
{
    VkFence fence;
    VkFenceCreateInfo fci = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VK_CHECK(vkCreateFence(ctx.device, &fci, nullptr, &fence));

    VkQueue graphics_queue;
    vkGetDeviceQueue(ctx.device, ctx.graphics_family_idx, 0, &graphics_queue);

    VK_CHECK(vkEndCommandBuffer(vkhandle));

    VkSubmitInfo si       = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.commandBufferCount = 1;
    si.pCommandBuffers    = &vkhandle;

    VK_CHECK(vkQueueSubmit(graphics_queue, 1, &si, fence));

    VK_CHECK(vkWaitForFences(ctx.device, 1, &fence, true, UINT64_MAX));
    vkDestroyFence(ctx.device, fence, nullptr);

    auto &frame_resource = ctx.frame_resources.get_current();
    vkFreeCommandBuffers(ctx.device, frame_resource.command_pool, 1, &vkhandle);
}

/// --- Circular buffers

CircularBufferPosition map_circular_buffer_internal(API &api, CircularBuffer &circular, usize len)
{
    Buffer &buffer        = api.get_buffer(circular.buffer_h);
    usize &current_offset = circular.offset;

    constexpr uint min_uniform_buffer_alignment = 256u;
    len                                         = round_up_to_alignment(min_uniform_buffer_alignment, len);

    if (current_offset + len > buffer.size)
    {
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

ShaderH API::create_shader(std::string_view path)
{
    Shader shader;
    shader.name     = path;
    shader.bytecode = tools::read_file(path);

    VkShaderModuleCreateInfo info = {.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    info.codeSize                 = shader.bytecode.size();
    info.pCode                    = reinterpret_cast<const u32 *>(shader.bytecode.data());

    VK_CHECK(vkCreateShaderModule(ctx.device, &info, nullptr, &shader.vkhandle));

    return shaders.add(std::move(shader));
}

Shader &API::get_shader(ShaderH H)
{
    assert(H.is_valid());
    return *shaders.get(H);
}

void destroy_shader_internal(API &api, Shader &shader)
{
    vkDestroyShaderModule(api.ctx.device, shader.vkhandle, nullptr);
}

void API::destroy_shader(ShaderH H)
{
    assert(H.is_valid());
    auto *shader = shaders.get(H);
    destroy_shader_internal(*this, *shader);
    shaders.remove(H);
}

/// --- Programs

void GraphicsProgramInfo::push_constant(PushConstantInfo &&push_constant)
{
    push_constants.push_back(std::move(push_constant));
}

void GraphicsProgramInfo::vertex_stride(u32 value) { vertex_buffer_info.stride = value; }

void GraphicsProgramInfo::vertex_info(VertexInfo &&info)
{
    vertex_buffer_info.vertices_info.push_back(std::move(info));
}

// assume binding_set.bindings_info is already populated
void init_binding_set(Context &ctx, ShaderBindingSet &binding_set)
{
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    std::vector<VkDescriptorBindingFlags> flags;
    bindings.reserve(binding_set.bindings_info.size());
    flags.reserve(binding_set.bindings_info.size());

    binding_set.binded_data.resize(binding_set.bindings_info.size());

    usize i = 0;
    for (const auto &info_binding : binding_set.bindings_info)
    {
        bindings.emplace_back();
        auto &binding = bindings.back();

        flags.emplace_back();
        auto &flag = flags.back();

        binding.binding         = info_binding.slot;
        binding.stageFlags      = info_binding.stages;
        binding.descriptorType  = info_binding.type;
        binding.descriptorCount = info_binding.count;

        if (binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
        {
            binding_set.dynamic_offsets.push_back(0);
            binding_set.dynamic_bindings.push_back(i);
        }

        if (info_binding.count > 1)
        {
            flag = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
        }
        i++;
    }

    VkDescriptorSetLayoutBindingFlagsCreateInfo flags_info
        = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO};
    flags_info.bindingCount  = static_cast<u32>(bindings.size());
    flags_info.pBindingFlags = flags.data();

    VkDescriptorSetLayoutCreateInfo layout_info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layout_info.pNext                           = &flags_info;
    layout_info.flags                           = {/*vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool*/};
    layout_info.bindingCount                    = static_cast<u32>(bindings.size());
    layout_info.pBindings                       = bindings.data();

    VK_CHECK(vkCreateDescriptorSetLayout(ctx.device, &layout_info, nullptr, &binding_set.descriptor_layout));
}

GraphicsProgramH API::create_program(GraphicsProgramInfo &&info)
{
    GraphicsProgram program;

    /// --- Create descriptor set layout
    constexpr usize shader_count = 3;
    std::array<SpvReflectShaderModule, shader_count> shader_modules;

    std::array<VkShaderStageFlags, shader_count> shader_stage_flags = {
        VK_SHADER_STAGE_VERTEX_BIT,
        VK_SHADER_STAGE_GEOMETRY_BIT,
        VK_SHADER_STAGE_FRAGMENT_BIT,
    };

    std::array<ShaderH, shader_count> shader_handles = {
        info.vertex_shader,
        info.geom_shader,
        info.fragment_shader,
    };

    SpvReflectResult result             = SPV_REFLECT_RESULT_SUCCESS;
    constexpr usize MAX_DESCRIPTOR_SETS = 3;
    std::array<std::vector<VkDescriptorSetLayoutBinding>, MAX_DESCRIPTOR_SETS> bindings_per_set;
    std::array<std::vector<int>, MAX_DESCRIPTOR_SETS> bindings_initialized_per_set;
    std::array<std::vector<VkDescriptorBindingFlags>, MAX_DESCRIPTOR_SETS> binding_flags_per_set;
    std::optional<PushConstantInfo> push_constant = {};

    for (uint i_shader = 0; i_shader < shader_count; i_shader++)
    {
        if (!shader_handles[i_shader].is_valid())
        {
            continue;
        }
        const auto &shader = get_shader(shader_handles[i_shader]);
        result
            = spvReflectCreateShaderModule(shader.bytecode.size(), shader.bytecode.data(), &shader_modules[i_shader]);
        assert(result == SPV_REFLECT_RESULT_SUCCESS);

        u32 count = 0;
        result    = spvReflectEnumerateDescriptorSets(&shader_modules[i_shader], &count, nullptr);
        assert(result == SPV_REFLECT_RESULT_SUCCESS);

        std::vector<SpvReflectDescriptorSet *> descriptor_sets(count);
        result = spvReflectEnumerateDescriptorSets(&shader_modules[i_shader], &count, descriptor_sets.data());
        assert(result == SPV_REFLECT_RESULT_SUCCESS);

        for (const auto *p_refl_set : descriptor_sets)
        {
            const SpvReflectDescriptorSet &refl_set = *p_refl_set;

            usize set_number = refl_set.set; // actual set index
            assert(set_number < MAX_DESCRIPTOR_SETS && "The engine only supports 3 descriptor sets.");

            for (u32 i_binding = 0; i_binding < refl_set.binding_count; i_binding++)
            {
                const SpvReflectDescriptorBinding &refl_binding = *(refl_set.bindings[i_binding]);

                auto slot = refl_binding.binding;

                if (slot >= bindings_per_set[set_number].size())
                {
                    bindings_per_set[set_number].resize(slot + 1);
                    bindings_initialized_per_set[set_number].resize(slot + 1);
                    binding_flags_per_set[set_number].resize(slot + 1);
                }

                auto &binding       = bindings_per_set[set_number][slot];
                auto &flag          = binding_flags_per_set[set_number][slot];
                int &is_initialized = bindings_initialized_per_set[set_number][slot];

                auto descriptorType = static_cast<VkDescriptorType>(refl_binding.descriptor_type);

                // all uniform buffers are dynamic
                if (descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
                {
                    descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
                }

                u32 descriptorCount = 1;
                for (u32 i_dim = 0; i_dim < refl_binding.array.dims_count; i_dim++)
                {
                    descriptorCount *= refl_binding.array.dims[i_dim];
                }
                auto stageFlags = static_cast<VkShaderStageFlagBits>(shader_modules[i_shader].shader_stage);

                if (is_initialized)
                {
                    binding.stageFlags |= shader_stage_flags[i_shader];
                    if (binding.binding != slot || binding.descriptorType != descriptorType
                        || binding.descriptorCount != descriptorCount)
                    {
                        assert(false && "The binding is different in another stage.");
                    }
                }
                else
                {
                    binding.binding         = slot;
                    binding.descriptorType  = descriptorType;
                    binding.descriptorCount = descriptorCount;
                    binding.stageFlags      = stageFlags;
                    flag                    = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
                    is_initialized          = 1;
                }
            }
        }

        u32 push_constant_count = 0;
        spvReflectEnumeratePushConstantBlocks(&shader_modules[i_shader], &push_constant_count, nullptr);
        assert(push_constant_count == 0 || push_constant_count == 1);

        SpvReflectBlockVariable *p_pc_block = nullptr;
        spvReflectEnumeratePushConstantBlocks(&shader_modules[i_shader], &push_constant_count, &p_pc_block);

        if (p_pc_block)
        {
            if (push_constant)
            {
                push_constant->stages |= shader_stage_flags[i_shader];
                if (p_pc_block->offset != push_constant->offset || p_pc_block->size != push_constant->size)
                {
                    assert(false && "The push constant is different in another stage.");
                }
            }
            else
            {
                push_constant = PushConstantInfo{
                    .stages = shader_stage_flags[i_shader],
                    .offset = p_pc_block->offset,
                    .size   = p_pc_block->size,
                };
            }
        }
        spvReflectDestroyShaderModule(&shader_modules[i_shader]);
    }

    if (push_constant)
    {
        info.push_constants.push_back(*push_constant);
    }

    // Init binding set
    for (usize i_set = SHADER_DESCRIPTOR_SET; i_set < bindings_per_set.size(); i_set++)
    {
        auto &binding_set = program.binding_sets_by_freq[i_set - 1];

        const auto &flags    = binding_flags_per_set[i_set];
        const auto &bindings = bindings_per_set[i_set];
        binding_set.binded_data.resize(bindings.size());

        binding_set.bindings_info.resize(bindings.size());
        for (usize i_binding = 0; i_binding < bindings.size(); i_binding++)
        {
            const auto &binding = bindings[i_binding];

            auto &binding_info  = binding_set.bindings_info[i_binding];
            binding_info.count  = binding.descriptorCount;
            binding_info.set    = i_set;
            binding_info.slot   = i_binding;
            binding_info.stages = binding.stageFlags;
            binding_info.type   = binding.descriptorType;

            if (binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
            {
                binding_set.dynamic_offsets.push_back(0);
                binding_set.dynamic_bindings.push_back(i_binding);
            }
        }

        VkDescriptorSetLayoutBindingFlagsCreateInfo flags_info
            = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO};
        flags_info.bindingCount  = static_cast<u32>(bindings.size());
        flags_info.pBindingFlags = flags.data();

        VkDescriptorSetLayoutCreateInfo layout_info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        layout_info.pNext                           = &flags_info;
        layout_info.flags                           = {/*vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool*/};
        layout_info.bindingCount                    = static_cast<u32>(bindings.size());
        layout_info.pBindings                       = bindings.data();

        VK_CHECK(vkCreateDescriptorSetLayout(ctx.device, &layout_info, nullptr, &binding_set.descriptor_layout));
    }

    /// ---

    /// --- Create pipeline layout

    std::vector<VkPushConstantRange> pc_ranges;
    map_transform(info.push_constants, pc_ranges, [](const auto &push_constant) {
        VkPushConstantRange range;
        range.stageFlags = push_constant.stages;
        range.offset     = push_constant.offset;
        range.size       = push_constant.size;
        return range;
    });

    std::array<VkDescriptorSetLayout, MAX_DESCRIPTOR_SET + 1 /*global set*/> layouts;
    layouts[0] = global_bindings.binding_set.descriptor_layout;
    for (uint i = 0; i < program.binding_sets_by_freq.size(); i++)
    {
        layouts[i + 1] = program.binding_sets_by_freq[i].descriptor_layout;
    }

    VkPipelineLayoutCreateInfo ci = {.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    ci.pSetLayouts                = layouts.data();
    ci.setLayoutCount             = static_cast<u32>(layouts.size());
    ci.pPushConstantRanges        = pc_ranges.data();
    ci.pushConstantRangeCount     = static_cast<u32>(pc_ranges.size());

    VK_CHECK(vkCreatePipelineLayout(ctx.device, &ci, nullptr, &program.pipeline_layout));
    program.info = std::move(info);

    return graphics_programs.add(std::move(program));
}

ComputeProgramH API::create_program(ComputeProgramInfo &&info)
{
    ComputeProgram program;

    /// --- Create descriptor set layout
    SpvReflectShaderModule shader_module;

    SpvReflectResult result = SPV_REFLECT_RESULT_SUCCESS;
    const auto &shader      = get_shader(info.shader);
    result = spvReflectCreateShaderModule(shader.bytecode.size(), shader.bytecode.data(), &shader_module);
    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    std::vector<VkDescriptorSetLayoutBinding> bindings;
    std::vector<VkDescriptorBindingFlags> binding_flags;

    u32 count = 0;
    result    = spvReflectEnumerateDescriptorSets(&shader_module, &count, nullptr);
    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    std::vector<SpvReflectDescriptorSet *> descriptor_sets(count);
    result = spvReflectEnumerateDescriptorSets(&shader_module, &count, descriptor_sets.data());
    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    for (usize i_set = 0; i_set < descriptor_sets.size(); i_set++)
    {
        const SpvReflectDescriptorSet &refl_set = *(descriptor_sets[i_set]);

        usize set_number = refl_set.set; // actual set index
        if (set_number != SHADER_DESCRIPTOR_SET)
            continue;

        for (u32 i_binding = 0; i_binding < refl_set.binding_count; i_binding++)
        {
            const SpvReflectDescriptorBinding &refl_binding = *(refl_set.bindings[i_binding]);

            auto slot = refl_binding.binding;

            if (slot >= bindings.size())
            {
                bindings.resize(slot + 1);
                binding_flags.resize(slot + 1);
            }

            auto &binding = bindings[slot];
            auto &flag    = binding_flags[slot];

            auto descriptorType = static_cast<VkDescriptorType>(refl_binding.descriptor_type);

            // all uniform buffers are dynamic
            if (descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
            {
                descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
            }

            u32 descriptorCount = 1;
            for (u32 i_dim = 0; i_dim < refl_binding.array.dims_count; i_dim++)
            {
                descriptorCount *= refl_binding.array.dims[i_dim];
            }
            auto stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

            binding.binding         = slot;
            binding.descriptorType  = descriptorType;
            binding.descriptorCount = descriptorCount;
            binding.stageFlags      = stageFlags;

            flag = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
        }
    }

    u32 push_constant_count = 0;
    spvReflectEnumeratePushConstantBlocks(&shader_module, &push_constant_count, nullptr);
    assert(push_constant_count == 0 || push_constant_count == 1);

    SpvReflectBlockVariable *p_pc_block = nullptr;
    spvReflectEnumeratePushConstantBlocks(&shader_module, &push_constant_count, &p_pc_block);

    if (p_pc_block)
    {
        program.info.push_constants.push_back(PushConstantInfo{
            .stages = VK_SHADER_STAGE_COMPUTE_BIT,
            .offset = p_pc_block->offset,
            .size   = p_pc_block->size,
        });
    }

    spvReflectDestroyShaderModule(&shader_module);

    // Init binding set
    auto &binding_set = program.binding_set;
    const auto &flags = binding_flags;

    binding_set.binded_data.resize(bindings.size());

    binding_set.bindings_info.resize(bindings.size());
    for (usize i_binding = 0; i_binding < bindings.size(); i_binding++)
    {
        const auto &binding = bindings[i_binding];

        auto &binding_info  = binding_set.bindings_info[i_binding];
        binding_info.count  = binding.descriptorCount;
        binding_info.set    = 1;
        binding_info.slot   = i_binding;
        binding_info.stages = binding.stageFlags;
        binding_info.type   = binding.descriptorType;

        if (binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
        {
            binding_set.dynamic_offsets.push_back(0);
            binding_set.dynamic_bindings.push_back(i_binding);
        }
    }

    VkDescriptorSetLayoutBindingFlagsCreateInfo flags_info
        = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO};
    flags_info.bindingCount  = static_cast<u32>(bindings.size());
    flags_info.pBindingFlags = flags.data();

    VkDescriptorSetLayoutCreateInfo layout_info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layout_info.pNext                           = &flags_info;
    layout_info.flags                           = {/*vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool*/};
    layout_info.bindingCount                    = static_cast<u32>(bindings.size());
    layout_info.pBindings                       = bindings.data();

    VK_CHECK(vkCreateDescriptorSetLayout(ctx.device, &layout_info, nullptr, &binding_set.descriptor_layout));

    /// --- Create pipeline layout

    std::vector<VkPushConstantRange> pc_ranges;
    map_transform(info.push_constants, pc_ranges, [](const auto &push_constant) {
        VkPushConstantRange range;
        range.stageFlags = push_constant.stages;
        range.offset     = push_constant.offset;
        range.size       = push_constant.size;
        return range;
    });

    std::array<VkDescriptorSetLayout, 1 + 1 /*global set*/> layouts = {
        global_bindings.binding_set.descriptor_layout,
        program.binding_set.descriptor_layout,
    };

    VkPipelineLayoutCreateInfo ci = {.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    ci.pSetLayouts                = layouts.data();
    ci.setLayoutCount             = layouts.size();
    ci.pPushConstantRanges        = pc_ranges.data();
    ci.pushConstantRangeCount     = static_cast<u32>(pc_ranges.size());

    VK_CHECK(vkCreatePipelineLayout(ctx.device, &ci, nullptr, &program.pipeline_layout));

    program.info = std::move(info);

    /// --- Create pipeline
    const auto &compute_shader = get_shader(program.info.shader);

    auto &pinfo        = program.pipeline_info;
    pinfo              = {.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    pinfo.stage        = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    pinfo.stage.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
    pinfo.stage.module = compute_shader.vkhandle;
    pinfo.stage.pName  = "main";
    pinfo.layout       = program.pipeline_layout;

    auto &pipeline = program.pipeline_vk;

    VK_CHECK(vkCreateComputePipelines(ctx.device, nullptr, 1, &pinfo, nullptr, &pipeline));
    compute_pipeline_count++;

    return compute_programs.add(std::move(program));
}

void ComputeProgramInfo::push_constant(PushConstantInfo &&push_constant)
{
    push_constants.push_back(std::move(push_constant));
}

GraphicsProgram &API::get_program(GraphicsProgramH H)
{
    assert(H.is_valid());
    return *graphics_programs.get(H);
}

ComputeProgram &API::get_program(ComputeProgramH H)
{
    assert(H.is_valid());
    return *compute_programs.get(H);
}

void GlobalBindings::binding(BindingInfo &&binding) { binding_set.bindings_info.push_back(std::move(binding)); }

void destroy_program_internal(API &api, GraphicsProgram &program)
{
    for (auto &binding_set : program.binding_sets_by_freq)
    {
        vkDestroyDescriptorSetLayout(api.ctx.device, binding_set.descriptor_layout, nullptr);
    }

    vkDestroyPipelineLayout(api.ctx.device, program.pipeline_layout, nullptr);

    for (VkPipeline pipeline : program.pipelines_vk)
    {
        vkDestroyPipeline(api.ctx.device, pipeline, nullptr);
        api.graphics_pipeline_count--;
    }
}

void API::destroy_program(GraphicsProgramH H)
{
    assert(H.is_valid());
    auto *program = graphics_programs.get(H);
    destroy_program_internal(*this, *program);
    graphics_programs.remove(H);
}

void destroy_program_internal(API &api, ComputeProgram &program)
{
    vkDestroyDescriptorSetLayout(api.ctx.device, program.binding_set.descriptor_layout, nullptr);
    vkDestroyPipelineLayout(api.ctx.device, program.pipeline_layout, nullptr);
    vkDestroyPipeline(api.ctx.device, program.pipeline_vk, nullptr);
    api.compute_pipeline_count--;
}

void API::destroy_program(ComputeProgramH H)
{
    assert(H.is_valid());
    auto *program = compute_programs.get(H);
    destroy_program_internal(*this, *program);
    compute_programs.remove(H);
}

void API::clear_image(ImageH H, const VkClearColorValue &clear_color)
{
    auto &frame_resource = ctx.frame_resources.get_current();
    VkCommandBuffer cmd  = frame_resource.command_buffer;
    auto &image          = get_image(H);

    auto src = get_src_image_access(image.usage);
    auto dst = get_dst_image_access(ImageUsage::TransferDst);

    VkImageMemoryBarrier b = {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    b.oldLayout            = src.layout;
    b.newLayout            = dst.layout;
    b.srcAccessMask        = src.access;
    b.dstAccessMask        = dst.access;
    b.image                = image.vkhandle;
    b.subresourceRange     = image.full_range;
    vkCmdPipelineBarrier(cmd, src.stage, dst.stage, 0, 0, nullptr, 0, nullptr, 1, &b);

    image.usage = ImageUsage::TransferDst;
    vkCmdClearColorImage(cmd, image.vkhandle, dst.layout, &clear_color, 1, &image.full_range);
}

static void clear_buffer_internal(API &api, Buffer &buffer, u32 data)
{
    auto &frame_resource = api.ctx.frame_resources.get_current();
    VkCommandBuffer cmd  = frame_resource.command_buffer;

    {
        VkBufferMemoryBarrier b = {.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
        b.srcAccessMask         = 0;
        b.dstAccessMask         = VK_ACCESS_TRANSFER_WRITE_BIT;
        b.buffer                = buffer.vkhandle;
        b.offset                = 0;
        b.size                  = buffer.size;

        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1, &b, 0, nullptr);
    }

    vkCmdFillBuffer(cmd, buffer.vkhandle, 0, buffer.size, data);


    {
        VkBufferMemoryBarrier b = {.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
        b.srcAccessMask         = VK_ACCESS_TRANSFER_WRITE_BIT;
        b.dstAccessMask         = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        b.buffer                = buffer.vkhandle;
        b.offset                = 0;
        b.size                  = buffer.size;

        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1, &b, 0, nullptr);
    }
}

void API::clear_buffer(BufferH H, u32 data)
{
    auto &buffer = get_buffer(H);
    clear_buffer_internal(*this, buffer, data);
}

void API::clear_buffer(BufferH H, float data)
{
    auto &buffer = get_buffer(H);
    auto *as_uint = reinterpret_cast<u32*>(&data);
    u32 data_uint = *as_uint;
    clear_buffer_internal(*this, buffer, data_uint);
}

} // namespace my_app::vulkan
