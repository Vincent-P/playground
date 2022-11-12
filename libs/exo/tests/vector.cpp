#include "exo/collections/vector.h"
#include "helpers.h"
#include <catch2/catch_test_macros.hpp>

struct DefaultValue
{
	bool is_default_constructed() const { return this->i == 33; }
	int  i = 33;
};

TEST_CASE("exo::Vec initializer list", "[vector]")
{
	exo::Vec<int> vector  = {1, 4, 2};
	const auto   &cvector = vector;

	REQUIRE(!cvector.is_empty());
	REQUIRE(cvector.len() == 3);
	REQUIRE(cvector[0] == 1);
	REQUIRE(cvector[1] == 4);
	REQUIRE(cvector[2] == 2);
}

TEST_CASE("exo::Vec with_capacity", "[vector]")
{
	auto vector = exo::Vec<int>::with_capacity(32);
	REQUIRE(vector.is_empty());
	REQUIRE(vector.len() == 0);
	REQUIRE(vector.capacity() == 32);
}

TEST_CASE("exo::Vec with_length", "[vector]")
{
	auto vector = exo::Vec<DefaultValue>::with_length(4);
	REQUIRE(!vector.is_empty());
	REQUIRE(vector.len() == 4);
	REQUIRE(vector.capacity() == 4);
	REQUIRE(vector[0].is_default_constructed());
	REQUIRE(vector[1].is_default_constructed());
	REQUIRE(vector[2].is_default_constructed());
	REQUIRE(vector[3].is_default_constructed());
}

TEST_CASE("exo::Vec with_values", "[vector]")
{
	DefaultValue not_default = {.i = 42};
	auto         vector      = exo::Vec<DefaultValue>::with_values(3, not_default);
	REQUIRE(!vector.is_empty());
	REQUIRE(vector.len() == 3);
	REQUIRE(vector.capacity() == 3);
	REQUIRE(!vector[0].is_default_constructed());
	REQUIRE(!vector[1].is_default_constructed());
	REQUIRE(!vector[2].is_default_constructed());
}

TEST_CASE("exo::Vec non-trivial objects", "[vector]")
{
	exo::Vec<DtorCalled> vector;
	vector.push(DtorCalled{.i = 0});
	vector.push(DtorCalled{.i = 1});
	vector.push(DtorCalled{.i = 2});
	vector.push(DtorCalled{.i = 3});

	REQUIRE(vector.len() == 4);
	REQUIRE(vector.capacity() == 4);

	SECTION("indexing elements")
	{
		REQUIRE(vector[0].i == 0);
		REQUIRE(vector[1].i == 1);
		REQUIRE(vector[2].i == 2);
		REQUIRE(vector[3].i == 3);
	}

	SECTION("swap remove")
	{
		vector.swap_remove(1);
		REQUIRE(vector.len() == 3);
		REQUIRE(vector.capacity() == 4);
		REQUIRE(vector[0].i == 0);
		REQUIRE(vector[1].i == 3);
		REQUIRE(vector[2].i == 2);
		REQUIRE(vector.data()[3].dtor_has_been_called());
	}

	SECTION("clear dtor")
	{
		vector.clear();
		REQUIRE(vector.is_empty());
		REQUIRE(vector.capacity() == 4);
		REQUIRE(vector.data()[0].dtor_has_been_called());
		REQUIRE(vector.data()[1].dtor_has_been_called());
		REQUIRE(vector.data()[2].dtor_has_been_called());
		REQUIRE(vector.data()[3].dtor_has_been_called());
	}
}

TEST_CASE("exo::Vec move constructor", "[vector]")
{
	exo::Vec<int> vector = {};
	vector.push(10);
	vector.push(8);
	vector.push(6);
	vector.push(4);

	REQUIRE(vector.buffer.ptr != nullptr);

	auto new_vector = std::move(vector);

	REQUIRE(vector.buffer.ptr == nullptr);
	REQUIRE(new_vector.buffer.ptr != nullptr);
}

TEST_CASE("exo::Vec lifetimes", "[vector]")
{
	int             alives = 0;
	exo::Vec<Alive> vector;
	vector.push(Alive{&alives});
	vector.push(Alive{&alives});
	vector.push(Alive{&alives});
	vector.push(Alive{&alives});

	CHECK(alives == 4);

	vector.swap_remove(1);
	CHECK(alives == 3);

	{
		auto tmp = vector.pop();
	}
	CHECK(alives == 2);

	vector.clear();
	CHECK(alives == 0);
}
