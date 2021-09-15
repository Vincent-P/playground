#pragma once
#include <exo/handle.h>
#include <exo/collections/vector.h>
#include <unordered_map>

#include "render/vulkan/synchronization.h"

namespace vulkan { struct WorkPool; struct Buffer; struct Image; struct Device;}
namespace gfx = vulkan;
struct Texture;

enum struct UploadState
{
    Requested,
    Uploading,
    Done
};

struct BufferUpload
{
    u32 i_staging;     // index of staging area
    u64 transfer_id;   // upload frame, when the current frame is greater than the upload frame the upload is done
    UploadState state; // Resource state
    usize len;         // length in bytes of the upload
};

struct BufferUploads
{
    std::unordered_map<usize, BufferUpload> offsets;
};

struct ImageRegion
{
    u32 mip_level = 0;
    u32 layer     = 0;
    usize offset  = 0; // offset in the source buffer
};

struct ImageUpload
{
    u32 i_staging;     // index of staging area
    u64 transfer_id;   // upload frame, when the current frame is greater than the upload frame the upload is done
    UploadState state; // Resource state
    usize len;         // length in bytes of the upload
    Vec<ImageRegion> regions;
};

// CPU Memory staging area
struct StagingArea
{
    Handle<gfx::Buffer> buffer;
    usize size;
    bool in_use;
};

class Streamer
{
public:
    void init(gfx::Device *_device);
    void wait() const;
    void update(gfx::WorkPool &work_pool);
    void destroy();

    // Buffers
    void upload(Handle<gfx::Buffer> buffer, const void *data, usize len, usize dst_offset = 0);
    bool is_uploaded(Handle<gfx::Buffer> buffer);
    bool is_uploaded(Handle<gfx::Buffer> buffer, usize dst_offset);

    // Images
    void upload(Handle<gfx::Image> image, const void *data, usize len);
    void upload(Handle<gfx::Image> image, const Texture &texture);
    bool is_uploaded(Handle<gfx::Image> image);

    gfx::Device *device;
    gfx::Fence transfer_done;

    u64 current_transfer = 0;
    u64 transfer_batch = 0;

    Vec<StagingArea> staging_areas;
    usize cpu_memory_usage;

    std::unordered_map<Handle<gfx::Buffer>, BufferUploads> buffer_uploads;
    std::unordered_map<Handle<gfx::Image>, ImageUpload> image_uploads;
};
