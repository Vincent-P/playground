#include <vulkan/vulkan.hpp>
#include "renderer/hl_api.hpp"

namespace my_app::vulkan
{
    RenderTargetH API::create_rendertarget(const RTInfo& info)
    {
        RenderTarget rt;
        rt.is_swapchain = info.is_swapchain;

        rendertargets.push_back(rt);

        u32 h = static_cast<u32>(rendertargets.size()) - 1;
        return RenderTargetH(h);
    }

    RenderTarget& API::get_rendertarget(RenderTargetH H)
    {
        return rendertargets[H.value()];
    }
}
