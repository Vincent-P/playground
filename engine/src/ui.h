#pragma once
#include <exo/base/option.h>

namespace cross {struct Window;}
class Inputs;

namespace UI
{
struct Context;
using ImGuiWindowFlags = int;

extern Context* g_ui_context;

struct WindowImpl
{
    WindowImpl(const char *name, bool *p_open, ImGuiWindowFlags flags);
    ~WindowImpl();
    operator bool() const { return is_opened && is_visible; }
private:
    bool is_opened;
    bool is_visible;
};

void create_context(cross::Window *window, Inputs *inputs);
void destroy_context(Context *context = nullptr);

void display_ui();

void new_frame();

WindowImpl begin_window(const char *name, ImGuiWindowFlags flags = 0);
} // namespace UI
