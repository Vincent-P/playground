#include <exo/collections/pool.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("exo::Pool insertion")
{
	exo::Pool<int> pool;

	auto h1 = pool.add(42);
	auto h2 = pool.add(38);

	REQUIRE(h1.is_valid());
	REQUIRE(h1.get_index() == 0);

	REQUIRE(h2.is_valid());
	REQUIRE(h2.get_index() == 1);

	auto v1 = pool.get(h1);
	auto v2 = pool.get(h2);

	REQUIRE(v1 == 42);
	REQUIRE(v2 == 38);
}
