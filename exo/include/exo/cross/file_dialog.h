#pragma once
#include <filesystem>
#include <utility>
#include <string>

#include <exo/collections/vector.h>
#include <exo/base/option.h>

namespace cross
{
    // Extensions filters are pair of (description, filter) like {"Image", "*.png"} for example
    Option<std::filesystem::path> file_dialog(Vec<std::pair<std::string, std::string>> extensions = {});
};
