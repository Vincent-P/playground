#include "gltf.hpp"
#include <fstream>
#include <nlohmann/json.hpp>

namespace my_app
{
using json = nlohmann::json;

Model load_model(const char *path)
{
    Model model;

    std::ifstream f(path);
    json          j;
    f >> j;

    return model;
}
} // namespace my_app
