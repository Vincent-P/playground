#include "exo/path.h"
#include "exo/macros/assert.h"

#include <utility>
#include <xxhash.h>

static bool is_separator(char c) { return c == '/' || c == '\\'; }

static void append_path(exo::String &storage, exo::StringView to_append)
{
	// If the path to append starts with a separator, it is an absolute path
	if (is_separator(to_append[0])) {
		storage.clear();
	}

	storage.reserve(storage.len() + to_append.len() + 1);

	// If the left part doesn't have a separator, add one
	if (!storage.is_empty() && storage.back() != '/') {
		storage.push('/');
	}

	enum ParsingState
	{
		Eating,
		SkippingSeparator,
	};
	ParsingState state = Eating;

	const usize to_append_size = to_append.len();
	for (u32 i = 0; i < to_append_size; ++i) {
		auto current = to_append[i];
		auto next    = i + 1 < to_append_size ? to_append[i + 1] : '\0';

		if (current == '.') {
			// Skip './' or '.\\'
			if (is_separator(next)) {
				state = SkippingSeparator;
			} else {
				state = Eating;
				storage.push(current);
			}
		} else if (is_separator(current)) {
			// Skip empty separators '///' or '\\\\'
			if (is_separator(next)) {
				state = SkippingSeparator;
			}

			if (state == Eating) {
				storage.push('/');
			} else if (state == SkippingSeparator) {
				// skip
			}
		} else {
			// Not a separator anymore, resume eating
			if (state == SkippingSeparator) {
				state = Eating;
			}
			storage.push(current);
		}
	}
}

namespace exo
{
Path Path::from_string(exo::StringView path)
{
	Path res;
	append_path(res.str, path);
	return res;
}
Path Path::from_owned_string(exo::String &&str) { return Path::from_string(str); }

exo::StringView Path::extension() const
{
	u32       i_last_dot = u32_invalid;
	const u32 size       = u32(this->str.len());
	for (u32 i = 0; i < size; ++i) {
		if (this->str[size - i - 1] == '.') {
			i_last_dot = size - i - 1;
			break;
		}
	}

	if (i_last_dot == u32_invalid) {
		return exo::StringView{};
	}

	return exo::StringView{&this->str[i_last_dot], size - i_last_dot};
}

exo::StringView Path::filename() const
{
	u32       i_last_sep = u32_invalid;
	const u32 size       = u32(this->str.len());
	for (u32 i = 0; i < size; ++i) {
		if (this->str[size - i - 1] == '/') {
			i_last_sep = size - i - 1;
			break;
		}
	}

	if (i_last_sep == u32_invalid) {
		return exo::StringView{this->str};
	} else {
		const usize filename_length = size - (i_last_sep + 1);
		auto        filename        = exo::StringView{&this->str[i_last_sep + 1], filename_length};
		return filename;
	}
}

// static helpers
Path Path::join(exo::Path path, exo::StringView str)
{
	append_path(path.str, str);
	return path;
}

Path Path::join(exo::Path lhs, const exo::Path &rhs)
{
	append_path(lhs.str, rhs.str);
	return lhs;
}

Path Path::replace_filename(exo::Path path, exo::StringView new_filename)
{
	return Path::join(Path::remove_filename(std::move(path)), new_filename);
}

Path Path::remove_filename(exo::Path path)
{
	u32       i_last_sep = 0;
	const u32 size       = u32(path.str.len());
	for (u32 i = 0; i < size; ++i) {
		if (path.str[size - i - 1] == '/') {
			i_last_sep = size - i - 1;
			break;
		}
	}

	auto path_except_filename = exo::StringView{&path.str[0], i_last_sep + 1};
	return Path::from_string(path_except_filename);
}

[[nodiscard]] u64 hash_value(const exo::Path &path)
{
	const auto view = path.view();
	const u64  hash = XXH3_64bits(view.data(), view.len());
	return hash;
}

} // namespace exo
