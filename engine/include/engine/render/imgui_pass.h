#pragma once
struct BaseRenderer;
namespace vulkan { struct GraphicsWork; }
namespace vulkan { struct Device; }
namespace vulkan { struct Image; }
namespace vulkan { struct GraphicsProgram; }
namespace vulkan { struct Framebuffer; }
namespace gfx = vulkan;

#include <volk.h>

struct ImGuiPass
{
    Handle<gfx::GraphicsProgram> program;
    Handle<gfx::Image>  font_atlas;
};

void imgui_pass_init(gfx::Device &device, ImGuiPass &pass, VkFormat color_attachment_format);
void imgui_pass_draw(BaseRenderer &renderer, ImGuiPass &pass, gfx::GraphicsWork &cmd, Handle<gfx::Framebuffer> framebuffer);
