#include "exo/path.h"

#include "exo/macros/assert.h"

#include <xxhash.h>

static void append_path(std::string &storage, std::string_view to_append)
{
	storage.reserve(storage.size() + to_append.size() + 1);
	usize to_append_size = to_append.size();
	for (u32 i = 0; i < to_append_size; ++i) {
		if (to_append[i] == '\\') {
			storage.push_back('/');
		} else {
			storage.push_back(to_append[i]);
		}
	}
}

namespace exo
{
Path Path::from_string(std::string_view path)
{
	Path res;
	append_path(res.str, path);
	return res;
}
Path Path::from_owned_string(std::string &&str) { return Path::from_string(str); }

std::string_view Path::extension() const
{
	u32   i_last_dot = u32_invalid;
	usize size       = this->str.size();
	for (u32 i = 0; i < size; ++i) {
		if (this->str[size - i - 1] == '.') {
			i_last_dot = size - i - 1;
			break;
		}
	}

	if (i_last_dot == u32_invalid) {
		return std::string_view{};
	}

	return std::string_view{&this->str[i_last_dot], size - i_last_dot};
}

std::string_view Path::filename() const
{
	u32   i_last_dot = u32_invalid;
	u32   i_last_sep = 0;
	usize size       = this->str.size();
	for (u32 i = 0; i < size; ++i) {
		if (this->str[size - i - 1] == '.' && i_last_dot == u32_invalid) {
			i_last_dot = size - i - 1;
		}

		if (this->str[size - i - 1] == '/' && i_last_sep == 0) {
			i_last_sep = size - i - 1;
		}

		if (i_last_dot != u32_invalid && i_last_sep != 0) {
			break;
		}
	}

	usize filename_length = size - i_last_sep;
	if (i_last_dot != u32_invalid) {
		filename_length -= (size - i_last_dot + 1);
	}

	auto filename = std::string_view{&this->str[i_last_sep + 1], filename_length};
	return filename;
}

// static helpers
Path Path::join(exo::Path path, std::string_view str)
{
	append_path(path.str, str);
	return path;
}

Path Path::join(exo::Path lhs, exo::Path rhs)
{
	append_path(lhs.str, rhs.str);
	return lhs;
}

Path Path::replace_filename(exo::Path path, std::string_view new_filename)
{
	return Path::join(Path::remove_filename(path), new_filename);
}

Path Path::remove_filename(exo::Path path)
{
	u32   i_last_sep = 0;
	usize size       = path.str.size();
	for (u32 i = 0; i < size; ++i) {
		if (path.str[size - i - 1] == '/') {
			i_last_sep = size - i - 1;
			break;
		}
	}

	auto path_except_filename = std::string_view{&path.str[0], i_last_sep+1};
	return Path::from_string(path_except_filename);
}

u64 hash_value(const Path &path)
{
	const auto view = path.view();
	const u64  hash = XXH3_64bits(view.data(), view.size());
	return hash;
}
} // namespace exo
