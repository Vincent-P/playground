#include <exo/hash.h>

namespace exo
{
[[nodiscard]] u64 hash_value(int i)
{
	u64 hash = 0xdeadbeef;
	hash     = exo::hash_combine(hash, u64(i));
	return hash;
}
} // namespace exo

#include <exo/collections/map.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("map insertion")
{
	exo::Map<int, int> map = {};
	REQUIRE(map.size == 0);

	auto *inserted1 = map.insert(42, 37);
	REQUIRE(map.size == 1);
	REQUIRE(inserted1 != nullptr);
	REQUIRE(*inserted1 == 37);

	auto *inserted2 = map.insert(38, 41);
	REQUIRE(map.size == 2);
	REQUIRE(inserted2 != nullptr);
	REQUIRE(*inserted2 == 41);

	auto *value1 = map.at(42);
	REQUIRE(value1 != nullptr);
	REQUIRE(*value1 == 37);

	auto *value2 = map.at(38);
	REQUIRE(value2 != nullptr);
	REQUIRE(*value2 == 41);

	int key_sum   = 0;
	int value_sum = 0;
	for (const auto &[key, value] : map) {
		key_sum += key;
		value_sum += value;
	}
	REQUIRE(key_sum == (38 + 42));
	REQUIRE(value_sum == (37 + 41));

	map.remove(12); // removing an invalid key does not do anything
	REQUIRE(map.size == 2);

	map.remove(38);
	REQUIRE(map.size == 1);

	auto *removed_entry = map.at(38);
	REQUIRE(removed_entry == nullptr);

	map.remove(42);
	REQUIRE(map.size == 0);
}
