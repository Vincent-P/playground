#pragma once
#include "exo/maths/numerics.h"
#include "exo/string.h"
#include "exo/string_view.h"

namespace exo
{
struct Path
{
	exo::String str;

	Path() noexcept = default;

	static Path from_string(exo::StringView path);
	static Path from_owned_string(exo::String &&str);

	exo::StringView view() const { return exo::StringView{this->str}; }
	exo::StringView extension() const;
	exo::StringView filename() const;

	// static helpers
	static Path join(exo::Path path, exo::StringView str);
	static Path join(exo::Path lhs, const exo::Path &rhs);
	static Path replace_filename(exo::Path path, exo::StringView new_filename);
	static Path remove_filename(exo::Path path);
};

[[nodiscard]] inline u64 hash_value(const exo::Path &path) { return hash_value(exo::StringView{path.str}); }
} // namespace exo
