#include "cross/mapped_file.h"

#include "utils_win32.h"
#include <exo/macros/defer.h>

#include <windows.h>

namespace cross
{
MappedFile::MappedFile(MappedFile &&moved) noexcept { *this = std::move(moved); }

MappedFile::~MappedFile() { this->close(); }

MappedFile &MappedFile::operator=(MappedFile &&moved) noexcept
{
	if (this != &moved) {
		this->base_addr = std::exchange(moved.base_addr, nullptr);
		this->mapping   = std::exchange(moved.mapping, nullptr);
		this->size      = std::exchange(moved.size, 0);
	}
	return *this;
}

Option<MappedFile> MappedFile::open(const exo::StringView &path)
{
	MappedFile file{};

	auto utf16_path = utils::utf8_to_utf16(path);

	auto fd = CreateFile(utf16_path.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (!utils::is_handle_valid(fd)) {
		return {};
	}
	DEFER { CloseHandle(fd); };

	DWORD       hi = 0;
	const DWORD lo = GetFileSize(fd, &hi);
	file.size      = ((u64)hi << 32) | (u64)lo;

	file.mapping = CreateFileMapping(fd, nullptr, PAGE_READONLY, 0, 0, nullptr);
	if (!utils::is_handle_valid(file.mapping)) {
		return {};
	}

	file.base_addr = MapViewOfFile(file.mapping, FILE_MAP_READ, 0, 0, 0);
	if (!file.base_addr) {
		return {};
	}

	return file;
}

void MappedFile::close()
{
	if (base_addr) {
		UnmapViewOfFile(this->base_addr);
		CloseHandle(this->mapping);
		this->base_addr = nullptr;
		this->mapping   = nullptr;
	}
}
}; // namespace cross
