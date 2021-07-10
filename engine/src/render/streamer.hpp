#pragma once
#include "base/handle.hpp"
#include "base/vector.hpp"
#include <unordered_map>
#include "render/vulkan/commands.hpp"

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

    void upload(Handle<gfx::Buffer> buffer, const void *data, usize len);
    void upload(Handle<gfx::Image> image, const void *data, usize len);
    bool is_uploaded(Handle<gfx::Image> image);
    bool is_uploaded(Handle<gfx::Buffer> buffer);

    gfx::Device *device;
    gfx::Fence transfer_done;

    u64 current_transfer = 0;
    u64 transfer_batch = 0;

    Vec<StagingArea> staging_areas;
    usize cpu_memory_usage;

    std::unordered_map<Handle<gfx::Buffer>, ResourceUpload> buffer_uploads;
    std::unordered_map<Handle<gfx::Image>, ResourceUpload> image_uploads;
};
