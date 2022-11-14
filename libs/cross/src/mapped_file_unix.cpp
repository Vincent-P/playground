#include "exo/os/mapped_file.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace exo
{

MappedFile::MappedFile(MappedFile &&moved) { *this = std::move(moved); }

MappedFile::~MappedFile()
{
	if (this->fd > 0) {
		this->close();
	}
}

MappedFile &MappedFile::operator=(MappedFile &&moved)
{
	if (this != &moved) {
		fd        = std::exchange(moved.fd, -1);
		base_addr = std::exchange(moved.base_addr, nullptr);
		size      = std::exchange(moved.size, 0);
	}
	return *this;
}

Option<MappedFile> MappedFile::open(const exo::StringView &path)
{
	MappedFile file{};

	file.fd        = -1;
	file.size      = 0;
	file.base_addr = nullptr;

	int res = 0;

	res = ::open(path.data(), O_RDONLY);
	if (res < 0) {
		return {};
	}

	file.fd = res;

	struct stat file_stat = {};
	res                   = fstat(file.fd, &file_stat);
	if (res < 0) {
		return {};
	}

	file.size = file_stat.st_size;

	file.base_addr = mmap(nullptr, file.size, PROT_READ, 0, file.fd, 0);
	if (reinterpret_cast<isize>(file.base_addr) < 0) {
		return {};
	}

	return file;
}

void MappedFile::close() { ::close(fd); }

}; // namespace exo
