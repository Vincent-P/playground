#pragma once
#include <base/types.hpp>
#include <base/option.hpp>
#include <string_view>

using HANDLE = void*;

namespace platform
{

struct MappedFile
{
    HANDLE fd = nullptr;
    HANDLE mapping = nullptr;
    const void *base_addr = nullptr;
    u64 size = 0;

    MappedFile() = default;
    MappedFile(const MappedFile &copied) = delete;
    MappedFile(MappedFile && moved);
    ~MappedFile();

    MappedFile &operator=(MappedFile && moved);

    static Option<MappedFile> open(const std::string_view &path);
    void close();
};

}; // namespace platform
