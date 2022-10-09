#include <exo/hash.h>

namespace exo
{
u64 hash_value(int i)
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

	auto *inserted1 = map.insert(42, 38);
	REQUIRE(map.size == 1);
	REQUIRE(inserted1 != nullptr);
	REQUIRE(*inserted1 == 38);

	auto *inserted2 = map.insert(38, 42);
	REQUIRE(map.size == 2);
	REQUIRE(inserted2 != nullptr);
	REQUIRE(*inserted2 == 42);

	auto *value1 = map.at(42);
	REQUIRE(value1 != nullptr);
	REQUIRE(*value1 == 38);

	auto *value2 = map.at(38);
	REQUIRE(value2 != nullptr);
	REQUIRE(*value2 == 42);

	map.remove(12); // removing an invalid key does not do anything
	REQUIRE(map.size == 2);

	map.remove(38);
	REQUIRE(map.size == 1);

	auto *removed_entry = map.at(38);
	REQUIRE(removed_entry == nullptr);

	map.remove(42);
	REQUIRE(map.size == 0);
}
