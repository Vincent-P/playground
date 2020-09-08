#include "renderer/hl_api.hpp"
#include "tools.hpp"
#include "types.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

namespace my_app::vulkan
{

/// --- Render Target

RenderTargetH API::create_rendertarget(const RTInfo &info)
{
    RenderTarget rt;
    rt.image_h      = info.image_h;

    return rendertargets.add(std::move(rt));
}

RenderTarget &API::get_rendertarget(RenderTargetH H)
{
    assert(H.is_valid());
    return *rendertargets.get(H);
}

void API::destroy_rendertarget(RenderTargetH H)
{
    assert(H.is_valid());
    rendertargets.remove(H);
}

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

Image create_image_internal(vulkan::Context &ctx, const ImageInfo &info, VkImage external = VK_NULL_HANDLE)
{
    Image img;

    img.name = info.name;
    img.info = info;
    img.is_proxy = external != VK_NULL_HANDLE;

    img.extra_formats    = info.extra_formats;

    assert(info.mip_levels == 1 || !info.generate_mip_levels);

    VkImageCreateInfo image_info = { .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };

    if (info.is_sparse)
    {
        image_info.flags = VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT | VK_IMAGE_CREATE_SPARSE_BINDING_BIT;
    }
    if (!info.extra_formats.empty())
    {
        image_info.flags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
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
    image_info.tiling                = info.is_linear ? VK_IMAGE_TILING_LINEAR : VK_IMAGE_TILING_OPTIMAL;

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
    else if (!info.is_sparse)
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
    else
    {
        VkPhysicalDeviceSparseImageFormatInfo2 info2
            = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SPARSE_IMAGE_FORMAT_INFO_2};
        info2.format  = image_info.format;
        info2.type    = image_info.imageType;
        info2.samples = image_info.samples;
        info2.usage   = image_info.usage;
        info2.tiling  = image_info.tiling;

        uint props_count = 0;
        std::vector<VkSparseImageFormatProperties2> props;
        vkGetPhysicalDeviceSparseImageFormatProperties2(ctx.physical_device, &info2, &props_count, nullptr);
        props.resize(props_count);
        vkGetPhysicalDeviceSparseImageFormatProperties2(ctx.physical_device, &info2, &props_count, nullptr);

        assert(!props.empty() && info.max_sparse_size != 0);

        VK_CHECK(vkCreateImage(ctx.device, &image_info, nullptr, &img.vkhandle));

        VkMemoryRequirements mem_req;
        vkGetImageMemoryRequirements(ctx.device, img.vkhandle, &mem_req);

        uint sparse_mem_req_count = 0;
        std::vector<VkSparseImageMemoryRequirements> sparse_mem_req;
        vkGetImageSparseMemoryRequirements(ctx.device, img.vkhandle, &sparse_mem_req_count, nullptr);
        sparse_mem_req.resize(sparse_mem_req_count);
        vkGetImageSparseMemoryRequirements(ctx.device, img.vkhandle, &sparse_mem_req_count, sparse_mem_req.data());

        assert(!sparse_mem_req.empty());

        // According to Vulkan specification, for sparse resources memReq.alignment is also page size.
        const usize page_size = mem_req.alignment;

        // TODO: max_sparse_size might not be a multiple of page_size?
        assert(info.max_sparse_size % page_size == 0);
        const usize page_count = info.max_sparse_size / page_size;

        VkMemoryRequirements page_mem_req = mem_req;
        page_mem_req.size                 = page_size;

        VmaAllocationCreateInfo allocCreateInfo = {};
        allocCreateInfo.usage                   = VMA_MEMORY_USAGE_GPU_ONLY;

        img.sparse_allocations.resize(page_count);
        img.allocations_infos.resize(page_count);
        std::fill(img.sparse_allocations.begin(), img.sparse_allocations.end(), nullptr);
        VK_CHECK(vmaAllocateMemoryPages(ctx.allocator,
                                        &page_mem_req,
                                        &allocCreateInfo,
                                        page_count,
                                        img.sparse_allocations.data(),
                                        img.allocations_infos.data()));

        img.page_size = page_size;
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

    VkImageViewCreateInfo vci = {.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vci.flags                 = 0;
    vci.image                 = img.vkhandle;
    vci.format                = image_info.format;
    vci.components.r          = VK_COMPONENT_SWIZZLE_IDENTITY;
    vci.components.g          = VK_COMPONENT_SWIZZLE_IDENTITY;
    vci.components.b          = VK_COMPONENT_SWIZZLE_IDENTITY;
    vci.components.a          = VK_COMPONENT_SWIZZLE_IDENTITY;
    vci.subresourceRange      = img.full_range;
    vci.viewType              = view_type_from(image_info.imageType);

    VK_CHECK(vkCreateImageView(ctx.device, &vci, nullptr, &img.default_view));

    if (ENABLE_VALIDATION_LAYERS)
    {
        VkDebugUtilsObjectNameInfoEXT ni = {.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
        ni.objectHandle                  = reinterpret_cast<u64>(img.default_view);
        ni.objectType                    = VK_OBJECT_TYPE_IMAGE_VIEW;
        ni.pObjectName                   = info.name;
        VK_CHECK(ctx.vkSetDebugUtilsObjectNameEXT(ctx.device, &ni));
    }

    img.format_views.reserve(info.extra_formats.size());
    for (const auto &extra_format : info.extra_formats)
    {
        VkImageView format_view;
        vci.format = extra_format;
        VK_CHECK(vkCreateImageView(ctx.device, &vci, nullptr, &format_view));
        img.format_views.push_back(format_view);
    }

    vci.format = image_info.format;
    for (u32 i = 0; i < image_info.mipLevels; i++)
    {
        VkImageView mip_view;
        vci.subresourceRange.baseMipLevel = i;
        vci.subresourceRange.levelCount   = 1;
        VK_CHECK(vkCreateImageView(ctx.device, &vci, nullptr, &mip_view));
        img.mip_views.push_back(mip_view);
    }
    return img;
}

ImageH API::create_image(const ImageInfo &info)
{
    return images.add(create_image_internal(ctx, info));
}

ImageH API::create_image_proxy(VkImage external, const ImageInfo &info)
{
    return images.add(create_image_internal(ctx, info, external));
}

Image &API::get_image(ImageH H)
{
    assert(H.is_valid());
    return *images.get(H);
}

void destroy_image_internal(API &api, Image &img)
{
    if (img.mapped_ptr.data)
    {
        vmaUnmapMemory(api.ctx.allocator, img.allocation);
    }

    if (img.is_proxy)
    {
    }
    else if (!img.info.is_sparse)
    {
        vmaDestroyImage(api.ctx.allocator, img.vkhandle, img.allocation);
    }
    else
    {
        vkDestroyImage(api.ctx.device, img.vkhandle, nullptr);
        vmaFreeMemoryPages(api.ctx.allocator, img.sparse_allocations.size(), img.sparse_allocations.data());
    }

    vkDestroyImageView(api.ctx.device, img.default_view, nullptr);

    for (auto &image_view : img.format_views)
    {
        vkDestroyImageView(api.ctx.device, image_view, nullptr);
    }
    for (auto &image_view : img.mip_views)
    {
        vkDestroyImageView(api.ctx.device, image_view, nullptr);
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
        copy.imageExtent                     = { image.info.width, image.info.height, image.info.depth };
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
    auto b = get_image_barrier(image, src, dst);
    vkCmdPipelineBarrier(cmd_buffer.vkhandle, src.stage, dst.stage, 0, 0, nullptr, 0, nullptr, 1, &b);
    image.usage = ImageUsage::GraphicsShaderRead;
    cmd_buffer.submit_and_wait();
}


FatPtr API::read_image(ImageH H)
{
    auto &image = get_image(H);
    assert(!image.info.is_sparse);
    assert(image.info.is_linear);
    assert(image.info.mip_levels == 1);
    assert(image.info.memory_usage == VMA_MEMORY_USAGE_GPU_TO_CPU
           || image.info.memory_usage == VMA_MEMORY_USAGE_CPU_ONLY);

    if (!image.mapped_ptr.data)
    {
        vmaMapMemory(ctx.allocator, image.allocation, &image.mapped_ptr.data);
        // TODO: return size?
        image.mapped_ptr.size = 1;
    }

    assert(image.mapped_ptr.data);

    return image.mapped_ptr;
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
    sci.borderColor         = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
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

    shader.name = path;
    auto code   = tools::read_file(path);
    // keep code for reflection?

    VkShaderModuleCreateInfo info = {.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    info.codeSize                 = code.size();
    info.pCode                    = reinterpret_cast<const u32 *>(code.data());

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

void GraphicsProgramInfo::binding(BindingInfo &&binding)
{
    bindings_by_set[binding.set].push_back(std::move(binding));
}

void GraphicsProgramInfo::vertex_stride(u32 value)
{
    vertex_buffer_info.stride = value;
}

void GraphicsProgramInfo::vertex_info(VertexInfo &&info)
{
    vertex_buffer_info.vertices_info.push_back(std::move(info));
}

GraphicsProgramH API::create_program(GraphicsProgramInfo &&info)
{
    GraphicsProgram program;

    /// --- Create descriptor set layout

    for (uint i = 0; i < MAX_DESCRIPTOR_SET; i++)
    {
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        program.dynamic_count_by_set[i] = 0;

        map_transform(info.bindings_by_set[i], bindings, [&](const auto &info_binding) {
            VkDescriptorSetLayoutBinding binding{};
            binding.binding        = info_binding.slot;
            binding.stageFlags     = info_binding.stages;
            binding.descriptorType = info_binding.type;

            if (binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
            {
                program.dynamic_count_by_set[i]++;
            }

            binding.descriptorCount = info_binding.count;
            return binding;
        });

        // clang-format off
        std::vector<VkDescriptorBindingFlags> flags(info.bindings_by_set[i].size(), 0);
        // clang-format on

        VkDescriptorSetLayoutBindingFlagsCreateInfo flags_info
            = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO};
        flags_info.bindingCount  = static_cast<u32>(bindings.size());
        flags_info.pBindingFlags = flags.data();

        VkDescriptorSetLayoutCreateInfo layout_info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        layout_info.pNext                           = &flags_info;
        layout_info.flags                           = {/*vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool*/};
        layout_info.bindingCount                    = static_cast<u32>(bindings.size());
        layout_info.pBindings                       = bindings.data();

        VK_CHECK(vkCreateDescriptorSetLayout(ctx.device, &layout_info, nullptr, &program.descriptor_layouts[i]));
    }

    /// --- Create pipeline layout

    std::vector<VkPushConstantRange> pc_ranges;
    map_transform(info.push_constants, pc_ranges, [](const auto &push_constant) {
        VkPushConstantRange range;
        range.stageFlags = push_constant.stages;
        range.offset     = push_constant.offset;
        range.size       = push_constant.size;
        return range;
    });

    auto &layouts = program.descriptor_layouts;

    VkPipelineLayoutCreateInfo ci = {.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    ci.pSetLayouts                = layouts.data();
    ci.setLayoutCount             = static_cast<u32>(layouts.size());
    ci.pPushConstantRanges        = pc_ranges.data();
    ci.pushConstantRangeCount     = static_cast<u32>(pc_ranges.size());

    VK_CHECK(vkCreatePipelineLayout(ctx.device, &ci, nullptr, &program.pipeline_layout));
    program.info = std::move(info);

    for (uint i = 0; i < MAX_DESCRIPTOR_SET; i++)
    {
        program.data_dirty_by_set[i] = true;
    }

    return graphics_programs.add(std::move(program));
}

ComputeProgramH API::create_program(ComputeProgramInfo &&info)
{
    ComputeProgram program;

    /// --- Create descriptor set layout

    std::vector<VkDescriptorSetLayoutBinding> bindings;
    program.dynamic_count = 0;

    map_transform(info.bindings, bindings, [&](const auto &info_binding) {
        VkDescriptorSetLayoutBinding binding{};
        binding.binding        = info_binding.slot;
        binding.stageFlags     = info_binding.stages;
        binding.descriptorType = info_binding.type;

        if (binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
        {
            program.dynamic_count++;
        }

        binding.descriptorCount = info_binding.count;
        return binding;
    });

    // clang-format off
    std::vector<VkDescriptorBindingFlags> flags( info.bindings.size(), 0 );
    // clang-format on
    VkDescriptorSetLayoutBindingFlagsCreateInfo flags_info
        = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO};
    flags_info.bindingCount  = static_cast<u32>(bindings.size());
    flags_info.pBindingFlags = flags.data();

    VkDescriptorSetLayoutCreateInfo layout_info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layout_info.pNext        = &flags_info;
    layout_info.flags        = {/*vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool*/};
    layout_info.bindingCount = static_cast<u32>(bindings.size());
    layout_info.pBindings    = bindings.data();

    VK_CHECK(vkCreateDescriptorSetLayout(ctx.device, &layout_info, nullptr, &program.descriptor_layout));

    /// --- Create pipeline layout

    std::vector<VkPushConstantRange> pc_ranges;
    map_transform(info.push_constants, pc_ranges, [](const auto &push_constant) {
        VkPushConstantRange range;
        range.stageFlags = push_constant.stages;
        range.offset     = push_constant.offset;
        range.size       = push_constant.size;
        return range;
    });

    VkPipelineLayoutCreateInfo ci = {.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    ci.pSetLayouts                = &program.descriptor_layout;
    ci.setLayoutCount             = 1;
    ci.pPushConstantRanges        = pc_ranges.data();
    ci.pushConstantRangeCount     = static_cast<u32>(pc_ranges.size());

    VK_CHECK(vkCreatePipelineLayout(ctx.device, &ci, nullptr, &program.pipeline_layout));

    program.info = std::move(info);

    program.data_dirty = true;

    /// --- Create pipeline
    const auto &compute_shader = get_shader(program.info.shader);

    VkComputePipelineCreateInfo pinfo = {.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    pinfo.stage                       = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    pinfo.stage.stage                 = VK_SHADER_STAGE_COMPUTE_BIT;
    pinfo.stage.module                = compute_shader.vkhandle;
    pinfo.stage.pName                 = "main";
    pinfo.layout                      = program.pipeline_layout;

    program.pipelines_vk.emplace_back();
    auto &pipeline = program.pipelines_vk.back();

    VK_CHECK(vkCreateComputePipelines(ctx.device, nullptr, 1, &pinfo, nullptr, &pipeline));
    compute_pipeline_count++;

    program.pipelines_info.push_back(std::move(pinfo));

    return compute_programs.add(std::move(program));
}

void ComputeProgramInfo::push_constant(PushConstantInfo &&push_constant)
{
    push_constants.push_back(std::move(push_constant));
}

void ComputeProgramInfo::binding(BindingInfo &&binding)
{
    bindings.push_back(std::move(binding));
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

void destroy_program_internal(API &api, GraphicsProgram &program)
{
    for (VkDescriptorSetLayout layout : program.descriptor_layouts)
    {
        vkDestroyDescriptorSetLayout(api.ctx.device, layout, nullptr);
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
    vkDestroyDescriptorSetLayout(api.ctx.device, program.descriptor_layout, nullptr);
    vkDestroyPipelineLayout(api.ctx.device, program.pipeline_layout, nullptr);

    for (VkPipeline pipeline : program.pipelines_vk)
    {
        vkDestroyPipeline(api.ctx.device, pipeline, nullptr);
        api.compute_pipeline_count--;
    }
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
    VkCommandBuffer cmd             = frame_resource.command_buffer;
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

} // namespace my_app::vulkan
