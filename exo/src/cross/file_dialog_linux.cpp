#include "exo/cross/file_dialog.h"

#include <exo/prelude.h>

namespace cross
{
Option<std::filesystem::path> file_dialog(Vec<std::pair<std::string, std::string>>)
{
    return std::nullopt;
}
}
