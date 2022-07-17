#pragma once
#include <exo/collections/handle.h>

#include "render/vulkan/surface.h"
#include "render/vulkan/synchronization.h"

struct TextureDesc;
struct RenderGraph;

namespace builtins
{
struct SwapchainPass
{
	usize           i_frame = 0;
	vulkan::Fence   fence;
	vulkan::Surface surface;
};
Handle<TextureDesc> acquire_next_image(RenderGraph &graph, SwapchainPass &pass);
void                present(RenderGraph &graph, SwapchainPass &pass, u64 signal_value);

void copy_image(RenderGraph &graph, Handle<TextureDesc> src, Handle<TextureDesc> dst);
void blit_image(RenderGraph &graph, Handle<TextureDesc> src, Handle<TextureDesc> dst);
} // namespace builtins
