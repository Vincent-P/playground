#pragma once
#include "exo/string.h"
#include <utility>
#include "exo/collections/span.h"
#include "exo/option.h"

namespace cross
{
// Extensions filters are pair of (description, filter) like {"Image", "*.png"} for example
Option<exo::String> file_dialog(exo::Span<const std::pair<exo::String, exo::String>> extensions = {});
}; // namespace cross
