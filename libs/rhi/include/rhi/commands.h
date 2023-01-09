#pragma once
#include "exo/collections/dynamic_array.h"
#include "exo/collections/enum_array.h"
#include "exo/collections/handle.h"
#include "exo/collections/vector.h"
#include "exo/maths/vectors.h"
#include "exo/string_view.h"
#include "rhi/queues.h"
#include <vulkan/vulkan_core.h>

namespace rhi
{
enum struct ImageUsage : u8;
struct Context;
struct Surface;
inline constexpr usize MAX_SEMAPHORES = 4; // Maximum number of waitable semaphores per command buffer

// Command buffer / Queue abstraction
struct Work
{
	Context *ctx;
	VkCommandBuffer command_buffer;
	exo::DynamicArray<VkPipelineStageFlags, MAX_SEMAPHORES> wait_stage_list;

	// vulkan hacks:
	VkSemaphore image_acquired_semaphore;
	VkPipelineStageFlags image_acquired_stage;
	VkSemaphore signal_present_semaphore;

	// --
	void begin();
	void end();

	// vulkan hacks:
	void wait_for_acquired(Surface *surface, VkPipelineStageFlags stage_dst);
	void prepare_present(Surface *surface);

	// debug utils
	void begin_debug_label(exo::StringView label, float4 color = float4(0.0f));
	void end_debug_label();
};

} // namespace rhi
