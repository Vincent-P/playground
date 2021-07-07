#pragma once
#include <filesystem>
#include <utility>
#include <string>

#include "base/vector.hpp"
#include "base/option.hpp"

namespace platform
{
    // Extensions filters are pair of (description, filter) like {"Image", "*.png"} for example
    Option<std::filesystem::path> file_dialog(Vec<std::pair<std::string, std::string>> extensions = {});
};
