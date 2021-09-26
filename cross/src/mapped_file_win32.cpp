#include "cross/mapped_file.h"

#include <windows.h>

#include "utils_win32.h"

namespace platform
{

MappedFile::MappedFile(MappedFile &&moved)
{
    *this = std::move(moved);
}

MappedFile::~MappedFile()
{
    if (is_handle_valid(this->fd))
    {
        this->close();
    }
}

MappedFile &MappedFile::operator=(MappedFile &&moved)
{
    if (this != &moved)
    {
        fd        = std::exchange(moved.fd, nullptr);
        mapping   = std::exchange(moved.mapping, nullptr);
        base_addr = std::exchange(moved.base_addr, nullptr);
        size      = std::exchange(moved.size, 0);
    }
    return *this;
}

Option<MappedFile> MappedFile::open(const std::string_view &path)
{
    MappedFile file{};

    auto utf16_path = utf8_to_utf16(path);

    file.fd = CreateFile(utf16_path.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (!is_handle_valid(file.fd)) {
        return {};
    }
    DEFER { CloseHandle(file.fd); };

    DWORD hi = 0;
    DWORD lo = GetFileSize(file.fd, &hi);
    file.size = ((u64)hi << 32) | (u64)lo;

    file.mapping = CreateFileMapping(file.fd, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!is_handle_valid(file.mapping)) {
        return {};
    }
    DEFER { CloseHandle(file.mapping); };

    file.base_addr = MapViewOfFile(file.mapping, FILE_MAP_READ, 0, 0, 0);
    if (!file.base_addr) {
        return {};
    }

    return file;
}

void MappedFile::close()
{
    CloseHandle(fd);
    CloseHandle(mapping);

    fd = nullptr;
    mapping = nullptr;
}

}; // namespace platform
