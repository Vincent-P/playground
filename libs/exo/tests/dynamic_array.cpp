#include "exo/collections/dynamic_array.h"
#include "helpers.h"
#include <catch2/catch_test_macros.hpp>

struct DefaultValue
{
	bool is_default_constructed() const { return this->i == 33; }
	int  i = 33;
};

TEST_CASE("exo::DynamicArray default constructor", "[dynamic_array]")
{
	exo::DynamicArray<int, 4> array;
	const auto               &carray = array;

	REQUIRE(carray.is_empty());
	REQUIRE(carray.len() == 0);
}

TEST_CASE("exo::DynamicArray from list", "[dynamic_array]")
{
	exo::DynamicArray<int, 4> array = {0, 1, 2};
	REQUIRE(!array.is_empty());
	REQUIRE(array.len() == 3);
	REQUIRE(array[1] == 1);
}

TEST_CASE("exo::DynamicArray copy", "[dynamic_array]")
{
	exo::DynamicArray<int, 4> array = {0, 1, 2};

	auto                      copy  = array;


	REQUIRE(!copy.is_empty());
	REQUIRE(copy.len() == 3);
	REQUIRE(copy[1] == 1);
}

TEST_CASE("exo::DynamicArray move", "[dynamic_array]")
{
	exo::DynamicArray<int, 4> array = {0, 1, 2};

	auto                      copy  = array;


	REQUIRE(array.is_empty());
	REQUIRE(copy.len() == 3);
	REQUIRE(copy[1] == 1);
}
