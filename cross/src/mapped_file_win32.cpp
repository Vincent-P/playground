#include "exo/os/mapped_file.h"

#include "utils_win32.h"
#include <exo/macros/defer.h>

#include <windows.h>

namespace exo
{

MappedFile::MappedFile(MappedFile &&moved) { *this = std::move(moved); }

MappedFile::~MappedFile() { this->close(); }

MappedFile &MappedFile::operator=(MappedFile &&moved)
{
	if (this != &moved) {
		base_addr = std::exchange(moved.base_addr, nullptr);
		size      = std::exchange(moved.size, 0);
	}
	return *this;
}

Option<MappedFile> MappedFile::open(const std::string_view &path)
{
	MappedFile file{};

	auto utf16_path = utf8_to_utf16(path);

	auto fd = CreateFile(utf16_path.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (!is_handle_valid(fd)) {
		return {};
	}
	DEFER { CloseHandle(fd); };

	DWORD hi  = 0;
	DWORD lo  = GetFileSize(fd, &hi);
	file.size = ((u64)hi << 32) | (u64)lo;

	auto mapping = CreateFileMapping(fd, nullptr, PAGE_READONLY, 0, 0, nullptr);
	if (!is_handle_valid(mapping)) {
		return {};
	}
	DEFER { CloseHandle(mapping); };

	file.base_addr = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
	if (!file.base_addr) {
		return {};
	}

	return file;
}

void MappedFile::close()
{
	if (base_addr) {
		UnmapViewOfFile(base_addr);
		base_addr = nullptr;
	}
}

}; // namespace exo
