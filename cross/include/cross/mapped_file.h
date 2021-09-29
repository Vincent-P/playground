#pragma once
#include <exo/maths/numerics.h>
#include <exo/option.h>
#include "cross/prelude.h"
#include <string_view>

namespace cross
{

struct MappedFile
{
#if defined (CROSS_WINDOWS)
    void* fd = nullptr;
    void* mapping = nullptr;
#else
    int fd = -1;
#endif

    const void *base_addr = nullptr;
    usize size = 0;

    MappedFile() = default;
    MappedFile(const MappedFile &copied) = delete;
    MappedFile(MappedFile && moved);
    ~MappedFile();

    MappedFile &operator=(MappedFile && moved);

    static Option<MappedFile> open(const std::string_view &path);
    void close();
};

}; // namespace cross
