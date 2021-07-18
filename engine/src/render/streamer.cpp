#include "render/streamer.h"
#include "render/vulkan/device.h"

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
    for (auto &[dst_buffer, upload] : buffer_uploads)
    {
        if (upload.state == UploadState::Uploading && upload.transfer_id < transfer_batch)
        {
            auto &staging = staging_areas[upload.i_staging];
            upload.state = UploadState::Done;
            staging.in_use = true;
        }
    }
    for (auto &[dst_image, upload] : image_uploads)
    {
        if (upload.state == UploadState::Uploading && upload.transfer_id < transfer_batch)
        {
            auto &staging = staging_areas[upload.i_staging];
            upload.state = UploadState::Done;
            staging.in_use = true;
        }
    }

    // Don't start an empty command buffer
    if (current_transfer == transfer_batch) { return; }


    gfx::TransferWork transfer_cmd = device->get_transfer_work(work_pool);
    transfer_cmd.begin();

    for (auto &[dst_buffer, upload] : buffer_uploads)
    {
        if (upload.state == UploadState::Requested)
        {
            auto &staging = staging_areas[upload.i_staging];
            transfer_cmd.copy_buffer(staging.buffer, dst_buffer);
            upload.state = UploadState::Uploading;
        }
    }

    for (auto &[dst_image, upload] : image_uploads)
    {
        if (upload.state == UploadState::Requested)
        {
            auto &staging = staging_areas[upload.i_staging];
            transfer_cmd.clear_barrier(dst_image, gfx::ImageUsage::TransferDst);
            transfer_cmd.copy_buffer_to_image(staging.buffer, dst_image);
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
        if (!staging.in_use && staging.size <= len)
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

static ResourceUpload upload_resource(Streamer &streamer, const void *data, usize len)
{
    u32 i_staging = find_or_create_staging(streamer, len);
    auto &staging = streamer.staging_areas[i_staging];

    // Copy the source data into the staging
    void *dst = streamer.device->map_buffer(staging.buffer);
    std::memcpy(dst, data, len);

    ResourceUpload upload = {};
    upload.i_staging = i_staging;
    upload.transfer_id = streamer.current_transfer;
    upload.state = UploadState::Requested;

    streamer.current_transfer += 1;
    return upload;
}

void Streamer::upload(Handle<gfx::Buffer> buffer, const void *data, usize len)
{
    buffer_uploads[buffer] = upload_resource(*this, data, len);
}

void Streamer::upload(Handle<gfx::Image> image, const void *data, usize len)
{
    image_uploads[image] = upload_resource(*this, data, len);
}

bool Streamer::is_uploaded(Handle<gfx::Buffer> buffer)
{
    return buffer_uploads.contains(buffer) && buffer_uploads.at(buffer).state == UploadState::Done;
}

bool Streamer::is_uploaded(Handle<gfx::Image> image)
{
    return image_uploads.contains(image) && image_uploads.at(image).state == UploadState::Done;
}
