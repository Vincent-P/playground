#pragma once
#include <exo/maths/numerics.h>

namespace exo
{
struct String;

struct StringView
{
	const char *ptr    = nullptr;
	usize       length = 0;

	// --

	StringView() = default;
	StringView(const char *c_string);
	StringView(const char *c_string, usize len);
	StringView(const String &string);

	StringView(const StringView &other)            = default;
	StringView &operator=(const StringView &other) = default;

	StringView(StringView &&other) noexcept;
	StringView &operator=(StringView &&other) noexcept;

	// -- Element access

	const char &operator[](usize i) const;

	// -- Observers

	usize size() const { return this->length; }
	bool  is_empty() const { return this->length == 0; }

	// -- STL compat

	bool        empty() const { return this->is_empty(); }
	const char *data() const { return this->ptr; }
};

bool operator==(const StringView &lhs, const StringView &rhs);
bool operator==(const String &lhs, const StringView &rhs);
bool operator==(const StringView &lhs, const String &rhs);

template <usize N>
inline bool operator==(const StringView &view, const char (&literal)[N])
{
	// N includes the NULL terminator
	if (view.length != N - 1) {
		return false;
	}

	const char *data = view.data();
	for (usize i = 0; i < N - 1; ++i) {
		if (data[i] != literal[i]) {
			return false;
		}
	}

	return true;
}

template <usize N>
inline bool operator==(const char (&literal)[N], const StringView &view)
{
	return view == literal;
}

} // namespace exo
