#include "exo/collections/map.h"
#include "exo/hash.h"
#include "helpers.h"
#include <catch2/catch_test_macros.hpp>

// Provide a hash function
namespace exo
{
[[nodiscard]] u64 hash_value(int i)
{
	u64 hash = 0xdeadbeef;
	hash     = exo::hash_combine(hash, u64(i));
	return hash;
}
} // namespace exo

TEST_CASE("exo::Map at", "[map]")
{
	exo::Map<int, int> map;
	const auto        &cmap = map;

	REQUIRE(map.at(0) == nullptr);
	REQUIRE(map.at(123) == nullptr);
	REQUIRE(cmap.at(0) == nullptr);
	REQUIRE(cmap.at(123) == nullptr);
	REQUIRE(cmap.size == 0);

	map.insert(123, 333);

	REQUIRE(map.at(0) == nullptr);
	REQUIRE(*map.at(123) == 333);
	REQUIRE(cmap.at(0) == nullptr);
	REQUIRE(*cmap.at(123) == 333);
	REQUIRE(cmap.size == 1);
}

TEST_CASE("exo::Map foreach", "[map]")
{
	exo::Map<int, int> map;
	const auto        &cmap = map;

	constexpr int MAX_COUNT = 255;
	int           seed      = 0;
	for (int i = 0; i < MAX_COUNT; ++i) {
		seed = (seed + 1) * 15485863;
		map.insert(seed, i);
	}

	u32 iter = 0;
	int sum  = 0;
	for (const auto &[key, value] : cmap) {
		sum += value;
		++iter;
	}

	constexpr int MAX_COUNT_SUM = (MAX_COUNT * (MAX_COUNT - 1)) / 2;
	REQUIRE(iter == map.size);
	REQUIRE(iter == MAX_COUNT);
	REQUIRE(sum == MAX_COUNT_SUM);

	for (auto &[key, value] : map) {
		value = 0;
	}

	sum = 0;
	for (const auto &[key, value] : cmap) {
		sum += value;
	}
	REQUIRE(sum == 0);
}

TEST_CASE("exo::Map remove", "[map]")
{
	exo::Map<int, int> map = {};
	map.insert(1, 2);
	map.insert(3, 4);
	map.insert(4, 5);

	map.remove(3);

	REQUIRE(*map.at(1) == 2);
	REQUIRE(map.at(3) == nullptr);
	REQUIRE(*map.at(4) == 5);
	REQUIRE(map.size == 2);

	map.insert(6, 7);

	REQUIRE(*map.at(1) == 2);
	REQUIRE(map.at(3) == nullptr);
	REQUIRE(*map.at(4) == 5);
	REQUIRE(*map.at(6) == 7);
	REQUIRE(map.size == 3);

	map.remove(1);

	REQUIRE(map.at(1) == nullptr);
	REQUIRE(map.at(3) == nullptr);
	REQUIRE(*map.at(4) == 5);
	REQUIRE(*map.at(6) == 7);
	REQUIRE(map.size == 2);
}

TEST_CASE("exo::Map removes non-trivial objects", "[map]")
{
	auto  map = exo::Map<int, DtorCalled>::with_capacity(16);
	auto *e1  = map.insert(1, {});
	auto *e3  = map.insert(3, {});
	auto *e4  = map.insert(4, {});

	map.remove(3);

	REQUIRE(!e1->dtor_has_been_called());
	REQUIRE(e3->dtor_has_been_called());
	REQUIRE(!e4->dtor_has_been_called());

	map.clear();

	REQUIRE(e1->dtor_has_been_called());
	REQUIRE(e3->dtor_has_been_called());
	REQUIRE(e4->dtor_has_been_called());
}

TEST_CASE("exo::Map move constructor", "[map]")
{
	exo::Map<int, int> map = {};
	map.insert(10, 9);
	map.insert(8, 7);
	map.insert(6, 5);
	map.insert(4, 3);

	REQUIRE(map.keyvalues_buffer.ptr != nullptr);
	REQUIRE(map.slots_buffer.ptr != nullptr);

	auto new_map = std::move(map);

	REQUIRE(map.keyvalues_buffer.ptr == nullptr);
	REQUIRE(map.slots_buffer.ptr == nullptr);
	REQUIRE(new_map.keyvalues_buffer.ptr != nullptr);
	REQUIRE(new_map.slots_buffer.ptr != nullptr);
}
