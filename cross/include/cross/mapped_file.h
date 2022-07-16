#pragma once
#include "exo/os/prelude.h"
#include <exo/maths/numerics.h>
#include <exo/option.h>
#include <string_view>

namespace

{

struct MappedFile
{
#if defined(CROSS_WINDOWS)
#else
	int fd = -1;
#endif

	const void *base_addr = nullptr;
	usize       size      = 0;

	MappedFile()                         = default;
	MappedFile(const MappedFile &copied) = delete;
	MappedFile(MappedFile &&moved);
	~MappedFile();

	MappedFile &operator=(const MappedFile &copied) = delete;
	MappedFile &operator=(MappedFile &&moved);

	static Option<MappedFile> open(const std::string_view &path);
	void                      close();
};

}; // namespace
