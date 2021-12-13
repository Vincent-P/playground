#include "exo/os/file_dialog.h"

namespace exo::os
{
Option<std::filesystem::path> file_dialog(Vec<std::pair<std::string, std::string>>)
{
    return std::nullopt;
}
}
