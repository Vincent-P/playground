#include "exo/collections/span.h"
#include "exo/collections/dynamic_array.h"
#include "exo/collections/vector.h"
#include "helpers.h"
#include <catch2/catch_test_macros.hpp>

struct FakeCollection
{
	int       *data() { return &this->i[0]; }
	const int *data() const { return &this->i[0]; }

	size_t len() const { return 42; }

	operator exo::Span<int>() { return exo::Span<int>(this->data(), this->len()); }
	operator exo::Span<const int>() const { return exo::Span<const int>(this->data(), this->len()); }

	int i[42];
};

TEST_CASE("exo::Span from collection", "[span]")
{
	FakeCollection collection_mut   = {};
	const auto    &collection_const = collection_mut;

	{
		auto s = exo::Span<int>(collection_mut);
		REQUIRE(s.len() == collection_mut.len());

		REQUIRE(exo::Span<int>(collection_mut).len() == collection_mut.len());

		auto cs = exo::Span<const int>(collection_mut);
		REQUIRE(cs.len() == collection_mut.len());
	}

	{
		auto cs = exo::Span<const int>(collection_const);
		REQUIRE(cs.len() == collection_const.len());

		// Not possible!
		// auto s = exo::Span<int>(collection_const);
		// REQUIRE(s.len() == collection_const.len());

		REQUIRE(exo::Span<const int>(collection_const).len() == collection_const.len());
	}
}

TEST_CASE("exo::Span from dynamic array", "[span]")
{
	exo::DynamicArray<DtorCalled, 8> darray;
	darray.push_back({});

	exo::Span<const DtorCalled> cs = darray;
	REQUIRE(cs.len() == darray.size());

	exo::Span<DtorCalled> s = darray;
	REQUIRE(s.len() == darray.size());
}

TEST_CASE("exo::Span from vector", "[span]")
{
	exo::Vec<DtorCalled> vec;
	vec.push();

	exo::Span<const DtorCalled> cs = vec;
	REQUIRE(cs.len() == vec.len());

	exo::Span<DtorCalled> s = vec;
	REQUIRE(s.len() == vec.len());
}
