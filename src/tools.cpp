#include "tools.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace my_app::tools
{
namespace fs = std::filesystem;
std::vector<u8> read_file(const std::filesystem::path &path)
{
    std::ifstream file{path, std::ios::binary};
    if (file.fail())
    {
        throw std::runtime_error(std::string("Could not open \"" + path.string() + "\" file!").c_str());
    }

    std::streampos begin;
    std::streampos end;
    begin = file.tellg();
    file.seekg(0, std::ios::end);
    end = file.tellg();

    std::vector<u8> result(static_cast<usize>(end - begin));
    if (result.empty())
    {
        throw std::runtime_error(std::string("\"" + path.string() + "\" has a size of 0!").c_str());
    }

    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char *>(result.data()), end - begin);
    file.close();

    return result;
}

} // namespace my_app::tools
