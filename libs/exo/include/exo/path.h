#pragma once
#include <exo/maths/numerics.h>
#include <string>
#include <string_view>

namespace exo
{
struct Path
{
	std::string str;

	static Path from_string(std::string_view path);
	static Path from_owned_string(std::string &&str);

	std::string_view view() const { return std::string_view{this->str}; }
	std::string_view extension() const;
	std::string_view filename() const;

	// static helpers
	static Path join(exo::Path path, std::string_view str);
	static Path join(exo::Path lhs, const exo::Path &rhs);
	static Path replace_filename(exo::Path path, std::string_view new_filename);
	static Path remove_filename(exo::Path path);
};

[[nodiscard]] u64 hash_value(const exo::Path &path);
} // namespace exo
