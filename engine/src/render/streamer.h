#pragma once
#include <exo/handle.h>
#include <exo/collections/vector.h>
#include <unordered_map>
#include "render/vulkan/commands.h"

namespace vulkan { struct WorkPool;}
namespace gfx = vulkan;

enum struct UploadState
{
    Requested,
    Uploading,
    Done
};

// Buffer upload request
struct ResourceUpload
{
    u32 i_staging;
    u64 transfer_id;
    UploadState state;
    usize dst_offset;
    usize len;
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

    void upload(Handle<gfx::Buffer> buffer, const void *data, usize len, usize dst_offset = 0);
    void upload(Handle<gfx::Image> image, const void *data, usize len);
    bool is_uploaded(Handle<gfx::Image> image);
    bool is_uploaded(Handle<gfx::Buffer> buffer, usize len = 0, usize dst_offset = 0);

    gfx::Device *device;
    gfx::Fence transfer_done;

    u64 current_transfer = 0;
    u64 transfer_batch = 0;

    Vec<StagingArea> staging_areas;
    usize cpu_memory_usage;

    std::unordered_map<Handle<gfx::Buffer>, Vec<ResourceUpload>> buffer_uploads;
    std::unordered_map<Handle<gfx::Image>, ResourceUpload> image_uploads;
};
