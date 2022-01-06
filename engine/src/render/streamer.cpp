#include "render/streamer.h"

#include "assets/texture.h"

#include "render/vulkan/utils.h"
#include "render/vulkan/device.h"
#include "render/vulkan/commands.h"

#include <algorithm> // for std::max
#include <exo/collections/dynamic_array.h>

namespace
{
static bool copy_to_staging(Streamer &streamer, const void *data, usize len)
{
    if (len % 4 != 0) {
        len += 4 - (len % 4);
    }

    // wrap cursor at the end the buffer
    if (streamer.cursor + len > streamer.buffer_end)
    {
        streamer.cursor = streamer.buffer_start;
    }

    // check if there is enough space
    const auto update_size = streamer.update_start.size();
    const u8 *previous_update_start = streamer.update_start[(streamer.i_update+update_size-1)%update_size];
    if (previous_update_start && streamer.cursor < previous_update_start && streamer.cursor + len > previous_update_start)
    {
        return false;
    }

    std::memcpy(streamer.cursor, data, len);
    streamer.cursor += len;
    return true;
}
}

Streamer Streamer::create(gfx::Device *_device, u32 update_queue_length)
{
    Streamer result = {};
    result.device = _device;

    const usize buffer_capacity = 128_MiB;
    result.cpu_buffer = _device->create_buffer({
            .name         = "Streamer CPU buffer",
            .size         = buffer_capacity,
            .usage        = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            .memory_usage = VMA_MEMORY_USAGE_CPU_ONLY,
        });

    result.update_start.resize(update_queue_length+1);

    result.buffer_start = reinterpret_cast<u8*>(_device->map_buffer(result.cpu_buffer));
    result.buffer_end = result.buffer_start + buffer_capacity;
    result.cursor = result.buffer_start;

    return result;
}

void Streamer::destroy()
{
    device->destroy_buffer(cpu_buffer);
}

void Streamer::update(gfx::GraphicsWork &cmd)
{
    i_update += 1;
    update_start[i_update % update_start.size()] = cursor;

    cmd.begin_debug_label("Streamer::update");

    for (const auto &upload : image_region_uploads)
    {
        const auto *image = device->images.get(upload.image);

        Vec<VkBufferImageCopy> buffer_copy_regions;
        buffer_copy_regions.resize(upload.regions.size());
        for (u32 i_region = 0; i_region < buffer_copy_regions.size(); i_region++)
        {
            VkBufferImageCopy &buffer_copy_region              = buffer_copy_regions[i_region];

            buffer_copy_region                                 = {};
            buffer_copy_region.imageSubresource.aspectMask     = image->full_view.range.aspectMask;
            buffer_copy_region.imageSubresource.mipLevel       = upload.regions[i_region].mip_level;
            buffer_copy_region.imageSubresource.baseArrayLayer = upload.regions[i_region].layer;
            buffer_copy_region.imageSubresource.layerCount     = 1;
            buffer_copy_region.imageExtent.width               = upload.regions[i_region].image_size.x;
            buffer_copy_region.imageExtent.height              = upload.regions[i_region].image_size.y;
            buffer_copy_region.imageExtent.depth               = 1;
            buffer_copy_region.imageOffset.x                   = upload.regions[i_region].image_offset.x;
            buffer_copy_region.imageOffset.y                   = upload.regions[i_region].image_offset.y;
            buffer_copy_region.bufferOffset                    = upload.buffer_offset + upload.regions[i_region].buffer_offset;
            buffer_copy_region.bufferRowLength                 = upload.regions[i_region].buffer_size.x;
            buffer_copy_region.bufferImageHeight               = upload.regions[i_region].buffer_size.y;
        }

        cmd.barrier(upload.image, gfx::ImageUsage::TransferDst);
        cmd.copy_buffer_to_image(cpu_buffer, upload.image, buffer_copy_regions);
    }

    for (const auto &upload : buffer_region_uploads)
    {
        exo::DynamicArray<std::tuple<usize, usize, usize>, 8> regions;
        regions.resize(upload.regions.size());

        for (usize i_region = 0; i_region < regions.size(); i_region += 1)
        {
            std::get<0>(regions[i_region]) = upload.src_offset + upload.regions[i_region].src_offset;
            std::get<1>(regions[i_region]) = upload.regions[i_region].dst_offset;
            std::get<2>(regions[i_region]) = upload.regions[i_region].size;
        }

        cmd.copy_buffer(cpu_buffer, upload.buffer, regions);
    }

    image_region_uploads.clear();
    buffer_region_uploads.clear();

    cmd.end_debug_label();
}

bool Streamer::upload_image_full(Handle<gfx::Image> image, const void *data, usize len)
{
    const auto *gpu_image = device->images.get(image);
    const ImageRegion regions[] = {{.image_size = gpu_image->desc.size.xy()}};
    return upload_image_regions(image, data, len, regions);
}

bool Streamer::upload_image_regions(Handle<gfx::Image> image, const void *data, usize len, std::span<const ImageRegion> image_regions)
{
    const u8 *old_cursor = cursor;
    if (!copy_to_staging(*this, data, len))
    {
        return false;
    }

    auto &upload = exo::emplace_back(image_region_uploads);
    upload.image = image;
    upload.buffer_offset = old_cursor - buffer_start;
    for (const auto &region : image_regions)
    {
        upload.regions.push_back(region);
    }

    return true;
}

bool Streamer::upload_texture(Handle<gfx::Image> image, const Texture &texture)
{
    exo::DynamicArray<ImageRegion, 12> regions;
    regions.resize(texture.mip_offsets.size());
    for (u32 i_level = 0; i_level < texture.mip_offsets.size(); i_level += 1)
    {
        const i32 mip_width  = std::max(texture.width >> i_level, 1);
        const i32 mip_height = std::max(texture.height >> i_level, 1);

        regions[i_level].mip_level     = i_level;
        regions[i_level].layer         = 0;
        regions[i_level].buffer_offset = texture.mip_offsets[i_level];
        regions[i_level].image_size    = {mip_width, mip_height};
    }

    return upload_image_regions(image, texture.pixels_data, texture.data_size, regions);
}


bool Streamer::upload_buffer_regions(Handle<gfx::Buffer> buffer, const void *data, usize len, std::span<const BufferRegion> buffer_regions)
{
    const u8 *old_cursor = cursor;
    if (!copy_to_staging(*this, data, len))
    {
        return false;
    }

    auto &upload = exo::emplace_back(buffer_region_uploads);
    upload.buffer = buffer;
    upload.src_offset = old_cursor - buffer_start;
    for (const auto &region : buffer_regions)
    {
        upload.regions.push_back(region);
    }

    return true;
}

bool Streamer::upload_buffer_region(Handle<gfx::Buffer> buffer, const void *data, usize len, usize dst_offset)
{
    const BufferRegion regions[] = {{.dst_offset = dst_offset, .size = len}};
    return upload_buffer_regions(buffer, data, len, regions);
}
