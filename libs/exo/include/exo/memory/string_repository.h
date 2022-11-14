#pragma once
#include "exo/collections/map.h"
#include "exo/maths/numerics.h"

#include "exo/string_view.h"

/**
   A StringRepository is a string interner, it contains immutable strings.
   Strings interned in a repository can be compared using their pointers instead of memcmp, and all interned strings
sharing the same value will point to the same pointer.

   Individual strings CAN NOT be freed from the repository, but the entire repository can be freed at once.

   Reference: https://ourmachinery.com/post/data-structures-part-3-arrays-of-arrays/
**/

namespace exo
{
struct StringRepository
{
	static StringRepository create();
	static StringRepository with_capacity(usize capacity);
	~StringRepository();

	// Move-only struct
	StringRepository()                                         = default;
	StringRepository(const StringRepository &other)            = delete;
	StringRepository &operator=(const StringRepository &other) = delete;
	StringRepository(StringRepository &&other) noexcept;
	StringRepository &operator=(StringRepository &&other) noexcept;

	const char *intern(exo::StringView s);
	bool        is_interned(exo::StringView s);

private:
	Map<u64, u64> offsets       = {};
	char         *string_buffer = nullptr;
	usize         buffer_size   = 0;
};

inline StringRepository *tls_string_repository = nullptr;
} // namespace exo
