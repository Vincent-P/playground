#pragma once

#include "base/types.hpp"
#include "base/handle.hpp"

#include "render/vulkan/resources.hpp"
#include "render/vulkan/commands.hpp"

#include <unordered_map>
#include <unordered_set>

namespace vulkan {struct Buffer; struct Device; struct TransferWork;}

namespace gfx = vulkan;

/*
A pool of fixed size for gpu driven rendering (vertices, indices, materials, instances, etc)

possible to:
- allocate new data (n elements) -> returns an offset
- delete data (offset) -> void
- update data (offset, n elements) -> void
- upload changed data to GPU (upload the delta not the entire buffer!)
- query if offset is sent to gpu? (if model sent then draw model)
 */

struct GpuPoolDescription
{
    std::string_view name;
    u32 size;
    u32 element_size;
    u32 gpu_usage = gfx::storage_buffer_usage;
};

struct GpuPool
{
    struct FreeList
    {
        u32 size;
        u32 next;
    };

    std::string name;
    u32 size;
    u32 element_size;
    u32 capacity;

    u8 *data;
    u32 free_list_head_offset = 0;
    Handle<gfx::Buffer> host;
    Handle<gfx::Buffer> device;
    std::unordered_map<u32, u32> valid_allocations;
    std::unordered_set<u32> dirty_allocations;

    static GpuPool create(gfx::Device &device, const GpuPoolDescription &desc);
    std::pair<bool, u32> allocate(u32 element_count);
    void free(u32 offset);

    bool update(u32 offset, u32 element_count, void* data);
    bool is_up_to_date(u32 offset);

    inline bool has_changes() const { return dirty_allocations.empty() == false; }
    void upload_changes(gfx::TransferWork &cmd);

    inline void *operator[](u32 index)
    {
        return data + index * element_size;
    }

    inline const void *operator[](u32 index) const
    {
        return data + index * element_size;
    }

    template<typename T>
    T &get(u32 index) { return *reinterpret_cast<T*>(this->operator[](index)); }

    template<typename T>
    const T &get(u32 index) const { return *reinterpret_cast<const T*>(this->operator[](index)); }
};

struct StreamingBuffer
{
    u32 size;
    u32 element_size;
    uint current;
    uint capacity;
    u32  transfer_start = u32_invalid;
    u32  transfer_end = u32_invalid;
    u64  transfer_done = u32_invalid;
    Handle<gfx::Buffer> buffer;
    Handle<gfx::Buffer> buffer_staging;
};

StreamingBuffer streaming_buffer_create(gfx::Device &device, std::string_view name, u32 size, u32 element_size, u32 usage = gfx::storage_buffer_usage);
bool streaming_buffer_allocate(gfx::Device &device, StreamingBuffer &streaming_buffer, u32 nb_elements, u32 element_size, const void *src);
void streaming_buffer_upload(gfx::TransferWork &cmd, StreamingBuffer &streaming_buffer);
inline bool streaming_buffer_has_transfer(const StreamingBuffer &b) { return b.transfer_start != u32_invalid; }
