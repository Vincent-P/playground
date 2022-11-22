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

TEST_CASE("exo::DynamicArray move and copy constructors")
{
	exo::DynamicArray<int, 8> int_array;
	int_array.push(42);

	CHECK(int_array.len() == 1);
	CHECK(int_array[0] == 42);

	SECTION("Move ctor steals buffer")
	{
		auto new_array = std::move(int_array);

		CHECK(int_array.len() == 0);

		CHECK(new_array.len() == 1);
		CHECK(new_array[0] == 42);
	}

	SECTION("Copy ctor copy elements")
	{
		auto new_array = int_array;

		CHECK(int_array.len() == 1);
		CHECK(int_array[0] == 42);

		CHECK(new_array.len() == 1);
		CHECK(new_array[0] == 42);
	}
}

TEST_CASE("exo::DynamicArray Foreach")
{
	exo::DynamicArray<int, 8> int_array = {42, 38, 7};

	constexpr int EXPECTED_SUM = 42 + 38 + 7;

	SECTION("Const foreach")
	{
		const auto &const_array = int_array;
		int         sum         = 0;
		for (const auto &rValue : const_array)
			sum += rValue;

		CHECK(sum == EXPECTED_SUM);
	}

	SECTION("Non-const foreach")
	{
		auto &rNonConstVector = int_array;
		int   sum             = 0;
		for (int &rValue : rNonConstVector)
			sum += rValue;

		CHECK(sum == EXPECTED_SUM);
	}
}

TEST_CASE("exo::DynamicArray elements life-cycle")
{
	exo::DynamicArray<Alive, 8> objects;

	int counter = 0;

	objects.push(&counter);
	CHECK(counter == 1);

	objects.push(&counter);
	CHECK(counter == 2);

	objects.push(&counter);
	CHECK(counter == 3);

	objects.push(&counter);
	CHECK(counter == 4);

	objects.clear();
	CHECK(counter == 0);
}