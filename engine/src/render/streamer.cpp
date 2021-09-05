#include "render/streamer.h"
#include "render/vulkan/device.h"

#include <algorithm> // for std::max mdrr
#include <ktx.h>

void Streamer::init(gfx::Device *_device)
{
    device = _device;
    transfer_done = device->create_fence();
}

void Streamer::wait() const
{
    // The first batch doesn't need to wait
    if (transfer_batch > 0)
    {
        device->wait_for_fence(transfer_done, transfer_batch);
    }
}

void Streamer::destroy()
{
    device->destroy_fence(transfer_done);
}

void Streamer::update(gfx::WorkPool &work_pool)
{
    for (auto &[dst_buffer, uploads] : buffer_uploads)
    {
        for (auto &[dst_offset, upload] : uploads.offsets)
        {
            if (upload.state == UploadState::Uploading && upload.transfer_id < transfer_batch)
            {
                auto &staging = staging_areas[upload.i_staging];
                upload.state = UploadState::Done;
                staging.in_use = false;
            }
        }
    }

    for (auto &[dst_image, upload] : image_uploads)
    {
        if (upload.state == UploadState::Uploading && upload.transfer_id < transfer_batch)
        {
            auto &staging = staging_areas[upload.i_staging];
            upload.state = UploadState::Done;
            staging.in_use = false;
        }
    }

    // Don't start an empty command buffer
    if (current_transfer == transfer_batch) { return; }


    gfx::TransferWork transfer_cmd = device->get_transfer_work(work_pool);
    transfer_cmd.begin();

    for (auto &[dst_buffer, uploads] : buffer_uploads)
    {
        for (auto &[dst_offset, upload] : uploads.offsets)
        {
            if (upload.state == UploadState::Requested)
            {
                auto &staging = staging_areas[upload.i_staging];
                transfer_cmd.copy_buffer(staging.buffer, dst_buffer, {{0, dst_offset, upload.len}});
                upload.state = UploadState::Uploading;
            }
        }
    }

    for (auto &[dst_image, upload] : image_uploads)
    {
        if (upload.state == UploadState::Requested)
        {
            auto &staging = staging_areas[upload.i_staging];
            gfx::Image &image = *device->images.get(dst_image);

            Vec<VkBufferImageCopy> buffer_copy_regions;
            buffer_copy_regions.resize(upload.regions.size());
            for (u32 i_region = 0; i_region < buffer_copy_regions.size(); i_region++)
            {
                VkBufferImageCopy &buffer_copy_region              = buffer_copy_regions[i_region];
                buffer_copy_region                                 = {};
                buffer_copy_region.imageSubresource.aspectMask     = image.full_view.range.aspectMask;
                buffer_copy_region.imageSubresource.mipLevel       = upload.regions[i_region].mip_level;
                buffer_copy_region.imageSubresource.baseArrayLayer = upload.regions[i_region].layer;
                buffer_copy_region.imageSubresource.layerCount     = 1;
                buffer_copy_region.imageExtent.width               = std::max(image.desc.size.x >> buffer_copy_region.imageSubresource.mipLevel, 1u);
                buffer_copy_region.imageExtent.height              = std::max(image.desc.size.y >> buffer_copy_region.imageSubresource.mipLevel, 1u);
                buffer_copy_region.imageExtent.depth               = 1;
                buffer_copy_region.bufferOffset                    = upload.regions[i_region].offset;
            }

            transfer_cmd.clear_barrier(dst_image, gfx::ImageUsage::TransferDst);
            transfer_cmd.copy_buffer_to_image(staging.buffer, dst_image, buffer_copy_regions);
            upload.state = UploadState::Uploading;
        }
    }

    transfer_batch = current_transfer;

    transfer_cmd.end();
    device->submit(transfer_cmd, {transfer_done}, {transfer_batch});
}

static u32 find_or_create_staging(Streamer &streamer, usize len)
{
    for (u32 i = 0; i < streamer.staging_areas.size(); i += 1)
    {
        auto &staging = streamer.staging_areas[i];
        if (!staging.in_use && len <= staging.size)
        {
            staging.in_use = true;
            return i;
        }
    }

    auto &device = *streamer.device;

    StagingArea staging;

    staging.buffer = device.create_buffer({
            .name = "Staging buffer",
            .size = len,
            .usage = gfx::source_buffer_usage,
            .memory_usage = VMA_MEMORY_USAGE_CPU_ONLY,
        });
    staging.in_use = true;
    staging.size = len;

    streamer.staging_areas.push_back(staging);
    return static_cast<u32>(streamer.staging_areas.size()) - 1;
}

void Streamer::upload(Handle<gfx::Buffer> buffer, const void *data, usize len, usize dst_offset)
{
    u32 i_staging = find_or_create_staging(*this, len);
    auto &staging = staging_areas[i_staging];

    // Copy the source data into the staging
    void *dst = device->map_buffer(staging.buffer);
    std::memcpy(dst, data, len);

    BufferUpload &upload = buffer_uploads[buffer].offsets[dst_offset];
    upload.i_staging     = i_staging;
    upload.transfer_id   = current_transfer;
    upload.state         = UploadState::Requested;
    upload.len           = len;

    current_transfer += 1;
}

bool Streamer::is_uploaded(Handle<gfx::Buffer> buffer)
{
    if (!buffer_uploads.contains(buffer))
    {
        return false;
    }

    const auto &uploads = buffer_uploads[buffer].offsets;
    for (const auto &[offset, upload] : uploads)
    {
        if (upload.state != UploadState::Done)
        {
            return false;
        }
    }
    return true;
}

bool Streamer::is_uploaded(Handle<gfx::Buffer> buffer, usize dst_offset)
{
    return buffer_uploads.contains(buffer) && buffer_uploads[buffer].offsets.contains(dst_offset) && buffer_uploads[buffer].offsets[dst_offset].state == UploadState::Done;
}


void Streamer::upload(Handle<gfx::Image> image, const void *data, usize len)
{
    u32 i_staging = find_or_create_staging(*this, len);
    auto &staging = staging_areas[i_staging];

    // Copy the source data into the staging
    void *dst = device->map_buffer(staging.buffer);
    std::memcpy(dst, data, len);

    ImageUpload &upload  = image_uploads[image];
    upload.i_staging     = i_staging;
    upload.transfer_id   = current_transfer;
    upload.state         = UploadState::Requested;
    upload.len           = len;

    ImageRegion region = {};
    region.offset = 0;
    region.layer = 0;
    region.mip_level = 0;
    upload.regions.push_back(region);

    current_transfer += 1;
}

void Streamer::upload(Handle<gfx::Image> image, void *texture)
{
    ktxTexture2 *ktx_texture = reinterpret_cast<ktxTexture2*>(texture);

    u32 i_staging = find_or_create_staging(*this, ktx_texture->dataSize);
    auto &staging = staging_areas[i_staging];

    // Copy the source data into the staging
    void *dst = device->map_buffer(staging.buffer);
    std::memcpy(dst, ktx_texture->pData, ktx_texture->dataSize);

    ImageUpload &upload  = image_uploads[image];
    upload.i_staging     = i_staging;
    upload.transfer_id   = current_transfer;
    upload.state         = UploadState::Requested;
    upload.len           = ktx_texture->dataSize;

    auto &regions = upload.regions;
    regions.resize(ktx_texture->numLevels);
    for (u32 i_level = 0; i_level < ktx_texture->numLevels; i_level += 1)
    {
        KTX_error_code result = ktxTexture_GetImageOffset(reinterpret_cast<ktxTexture *>(ktx_texture), i_level, 0, 0, &regions[i_level].offset);
        assert(result == KTX_SUCCESS);
        regions[i_level].mip_level = i_level;
        regions[i_level].layer     = 0;
    }

    current_transfer += 1;
}

bool Streamer::is_uploaded(Handle<gfx::Image> image)
{
    return image_uploads.contains(image) && image_uploads.at(image).state == UploadState::Done;
}
