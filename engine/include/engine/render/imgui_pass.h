#pragma once
#include "engine/render/vulkan/image.h"
#include "engine/render/vulkan/pipelines.h"
struct BaseRenderer;
namespace vulkan { struct GraphicsWork; }
namespace gfx = vulkan;

struct ImGuiPass
{
    Handle<gfx::GraphicsProgram> program;
    Handle<gfx::Image>  font_atlas;
};

void imgui_pass_init(gfx::Device &device, ImGuiPass &pass, VkFormat color_attachment_format);
void imgui_pass_draw(BaseRenderer &renderer, ImGuiPass &pass, gfx::GraphicsWork &cmd, Handle<gfx::Framebuffer> framebuffer);
