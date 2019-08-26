#include "tools.hpp"

#include <fstream>
#include <iostream>

namespace  my_app::tools
{
    std::vector<char> readFile(const std::string& filename)
    {
        std::ifstream file{ filename, std::ios::binary };
        if (file.fail())
            throw std::runtime_error(std::string("Could not open \"" + filename + "\" file!").c_str());

        std::streampos begin, end;
        begin = file.tellg();
        file.seekg(0, std::ios::end);
        end = file.tellg();

        std::vector<char> result(static_cast<size_t>(end - begin));
        file.seekg(0, std::ios::beg);
        file.read(&result[0], end - begin);
        file.close();

        return result;
    }
}
