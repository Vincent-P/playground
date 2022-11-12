#pragma once
#include <exo/maths/numerics.h>
#include <exo/option.h>

#include <cross/prelude.h>

#include <span>
#include <string_view>

namespace cross
{
struct MappedFile
{
#if defined(CROSS_WINDOWS)
#else
	int fd = -1;
#endif

	const void *base_addr = nullptr;
	void       *mapping   = nullptr;
	usize       size      = 0;

	MappedFile() = default;
	~MappedFile();

	MappedFile(const MappedFile &copied)            = delete;
	MappedFile &operator=(const MappedFile &copied) = delete;

	MappedFile(MappedFile &&moved) noexcept;
	MappedFile &operator=(MappedFile &&moved) noexcept;

	static Option<MappedFile> open(const std::string_view &path);

	inline std::span<const u8> content()
	{
		return std::span<const u8>{reinterpret_cast<const u8 *>(this->base_addr), this->size};
	}

	void close();
};
}; // namespace cross
