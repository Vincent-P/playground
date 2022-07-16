#pragma once
#include "exo/maths/numerics.h"
#include <string_view>

namespace exo
{
struct UUID
{
	static constexpr usize STR_LEN      = 35;
	u32                    data[4]      = {};
	char                   str[STR_LEN] = {};

	static UUID create();
	static UUID from_string(const char *s, usize len);
	static UUID from_values(const u32 *values);

	bool operator==(const UUID &other) const = default;

	// clang-format off
    bool is_valid() const { return data[0] != 0 || data[1] != 0 || data[2] != 0 || data[3] != 0; }
    std::string_view as_string() const { return std::string_view{this->str, STR_LEN}; }
	// clang-format on

	friend usize hash_value(const UUID &uuid);
};

} // namespace exo
