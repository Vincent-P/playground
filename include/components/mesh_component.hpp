#pragma once
#include "base/handle.hpp"

#include <imgui/imgui.h>

namespace gltf {struct Model;}

struct MeshComponent
{
    Handle<gltf::Model> model_handle;

    static const char *type_name() { return "MeshComponent"; }

    inline void display_ui()
    {
        ImGui::Text("Model index: %u", model_handle.value());
    }
};
