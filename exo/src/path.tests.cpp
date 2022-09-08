#include "exo/path.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Path concatenation", "[path]")
{
	auto path = exo::Path::from_string("a");
	path      = exo::Path::join(path, "b");

	REQUIRE(path.str == "a/b");
}