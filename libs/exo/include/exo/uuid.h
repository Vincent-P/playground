#pragma once
#include "exo/maths/numerics.h"
#include "exo/string_view.h"

namespace exo
{
struct UUID
{
	static constexpr usize STR_LEN      = 35;
	u32                    data[4]      = {};
	char                   str[STR_LEN] = {};

	static UUID create();
	static UUID from_string(exo::StringView str);
	static UUID from_values(const u32 *values);

	bool operator==(const UUID &other) const = default;

	bool            is_valid() const { return data[0] != 0 || data[1] != 0 || data[2] != 0 || data[3] != 0; }
	exo::StringView as_string() const { return exo::StringView{this->str, STR_LEN}; }
};

[[nodiscard]] u64 hash_value(const exo::UUID &uuid);

} // namespace exo
void write_uuid_string(u32 (&data)[4], char (&str)[exo::UUID::STR_LEN]);
