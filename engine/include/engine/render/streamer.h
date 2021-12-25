#pragma once
#include <exo/collections/dynamic_array.h>
#include <exo/collections/handle.h>
#include <exo/collections/vector.h>

namespace vulkan { struct WorkPool; struct Buffer; struct Image; struct Device; struct GraphicsWork;}
namespace gfx = vulkan;
struct Texture;

struct ImageRegion
{
    u32       mip_level     = 0;
    u32       layer         = 0;
    exo::int2 image_offset  = {};
    exo::int2 image_size    = {};
    usize     buffer_offset = 0;
    exo::int2 buffer_size   = {};
};

struct BufferRegion
{
    usize src_offset = 0;
    usize dst_offset = 0;
    usize size = 0;
};

struct ImageRegionUpload
{
    Handle<gfx::Image> image;
    usize buffer_offset = 0;
    Vec<ImageRegion> regions = {};
};

struct BufferRegionUpload
{
    Handle<gfx::Buffer> buffer;
    usize src_offset = 0;
    Vec<BufferRegion> regions = {};
};

struct Streamer
{
    static Streamer create(gfx::Device *_device, u32 update_queue_length);
    void destroy();
    bool upload_image_full(Handle<gfx::Image> image, const void *data, usize len);
    bool upload_image_regions(Handle<gfx::Image> image, const void *data, usize len, std::span<const ImageRegion> image_regions);
    bool upload_texture(Handle<gfx::Image> image, const Texture &texture);
    bool upload_buffer_regions(Handle<gfx::Buffer> buffer, const void *data, usize len, std::span<const BufferRegion> buffer_regions);
    bool upload_buffer_region(Handle<gfx::Buffer> buffer, const void *data, usize len, usize dst_offset);
    void update(gfx::GraphicsWork &work);

    gfx::Device *device;
    u32 i_update = 0;
    u8 *cursor;
    u8 *buffer_start;
    u8 *buffer_end;
    exo::DynamicArray<u8*, 3> update_start;
    Handle<gfx::Buffer> cpu_buffer;
    Vec<ImageRegionUpload> image_region_uploads;
    Vec<BufferRegionUpload> buffer_region_uploads;
};
