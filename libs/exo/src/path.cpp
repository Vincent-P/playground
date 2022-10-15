#include "exo/path.h"

#include "exo/logger.h"
#include "exo/macros/assert.h"

#include <xxhash.h>

static bool is_separator(char c) { return c == '/' || c == '\\'; }

static void append_path(std::string &storage, std::string_view to_append)
{
	// If the path to append starts with a separator, it is an absolute path
	if (is_separator(to_append[0])) {
		storage.clear();
	}

	storage.reserve(storage.size() + to_append.size() + 1);

	// If the left part doesn't have a separator, add one
	if (!storage.empty() && storage.back() != '/') {
		storage.push_back('/');
	}

	enum ParsingState
	{
		Eating,
		SkippingSeparator,
	};
	ParsingState state = Eating;

	const usize to_append_size = to_append.size();
	for (u32 i = 0; i < to_append_size; ++i) {
		auto current = to_append[i];
		auto next    = i + 1 < to_append_size ? to_append[i + 1] : '\0';

		if (current == '.') {
			// Skip './' or '.\\'
			if (is_separator(next)) {
				state = SkippingSeparator;
			} else {
				state = Eating;
				storage.push_back(current);
			}
		} else if (is_separator(current)) {
			// Skip empty separators '///' or '\\\\'
			if (is_separator(next)) {
				state = SkippingSeparator;
			}

			if (state == Eating) {
				storage.push_back('/');
			} else if (state == SkippingSeparator) {
				// skip
			}
		} else {
			// Not a separator anymore, resume eating
			if (state == SkippingSeparator) {
				state = Eating;
			}
			storage.push_back(current);
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
	u32       i_last_dot = u32_invalid;
	const u32 size       = u32(this->str.size());
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
	u32       i_last_sep = u32_invalid;
	const u32 size       = u32(this->str.size());
	for (u32 i = 0; i < size; ++i) {
		if (this->str[size - i - 1] == '/') {
			i_last_sep = size - i - 1;
			break;
		}
	}

	if (i_last_sep == u32_invalid) {
		return std::string_view{this->str};
	} else {
		const usize filename_length = size - (i_last_sep + 1);
		auto        filename        = std::string_view{&this->str[i_last_sep + 1], filename_length};
		return filename;
	}
}

// static helpers
Path Path::join(exo::Path path, std::string_view str)
{
	append_path(path.str, str);
	return path;
}

Path Path::join(exo::Path lhs, const exo::Path &rhs)
{
	append_path(lhs.str, rhs.str);
	return lhs;
}

Path Path::replace_filename(exo::Path path, std::string_view new_filename)
{
	return Path::join(Path::remove_filename(std::move(path)), new_filename);
}

Path Path::remove_filename(exo::Path path)
{
	u32       i_last_sep = 0;
	const u32 size       = u32(path.str.size());
	for (u32 i = 0; i < size; ++i) {
		if (path.str[size - i - 1] == '/') {
			i_last_sep = size - i - 1;
			break;
		}
	}

	auto path_except_filename = std::string_view{&path.str[0], i_last_sep + 1};
	return Path::from_string(path_except_filename);
}

[[nodiscard]] u64 hash_value(const exo::Path &path)
{
	const auto view = path.view();
	const u64  hash = XXH3_64bits(view.data(), view.size());
	return hash;
}

} // namespace exo
