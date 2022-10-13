#include "exo/uuid.h"

#include "exo/uuid_formatter.h"

#include "exo/collections/map.h"
#include "exo/hash.h"
#include "exo/macros/assert.h"

#if defined(PLATFORM_WINDOWS)
#include <rpc.h>
#include <windows.h>
#endif
#include <fmt/core.h>

#include <span>

#pragma comment(lib, "rpcrt4.lib")

static void write_uuid_string(u32 (&data)[4], char (&str)[exo::UUID::STR_LEN])
{
	std::memset(str, 0, exo::UUID::STR_LEN);
	fmt::format_to(str, "{:08x}-{:08x}-{:08x}-{:08x}", data[0], data[1], data[2], data[3]);
}

namespace exo
{
UUID UUID::create()
{
	UUID new_uuid = {};

#if defined(PLATFORM_WINDOWS)
	auto *win32_uuid = reinterpret_cast<::UUID *>(&new_uuid.data);
	static_assert(sizeof(::UUID) == sizeof(u32) * 4);
	auto res = ::UuidCreate(win32_uuid);
	ASSERT(res == RPC_S_OK);
	ASSERT(new_uuid.is_valid());
#endif

	write_uuid_string(new_uuid.data, new_uuid.str);
	return new_uuid;
}

UUID UUID::from_string(std::string_view str)
{
	UUID new_uuid;

	ASSERT(str.length() == UUID::STR_LEN);

	// 83ce0c20-4bb21feb-e6957dbb-5fcc54d5
	for (usize i = 0; i < UUID::STR_LEN; i += 1) {
		if (i == 8 || i == 17 || i == 26) {
			ASSERT(str[i] == '-');
		} else {
			ASSERT(('0' <= str[i] && str[i] <= '9') || ('a' <= str[i] && str[i] <= 'f'));
		}

		new_uuid.str[i] = str[i];
	}

	for (usize i_data = 0; i_data < 4; i_data += 1) {
		// 0..8, 1..17, 18..26, 27..35
		for (usize i = (i_data * 9); i < (i_data + 1) * 8 + i_data; i += 1) {
			uint n                = static_cast<uint>(str[i] >= 'a' ? str[i] - 'a' + 10 : str[i] - '0');
			new_uuid.data[i_data] = new_uuid.data[i_data] * 16 + n;
		}
	}

	return new_uuid;
}

UUID UUID::from_values(const u32 *values)
{
	UUID new_uuid;
	for (usize i_data = 0; i_data < 4; i_data += 1) {
		new_uuid.data[i_data] = values[i_data];
	}
	write_uuid_string(new_uuid.data, new_uuid.str);
	return new_uuid;
}
} // namespace exo

u64 hash_value(const exo::UUID &uuid)
{
	u64 hash = uuid.data[0];
	hash     = exo::hash_combine(hash, u64(uuid.data[1]));
	hash     = exo::hash_combine(hash, u64(uuid.data[2]));
	hash     = exo::hash_combine(hash, u64(uuid.data[3]));
	hash     = exo::hash_combine(hash, u64(uuid.data[4]));
	return hash;
}
