#include "exo/string.h"
#include "exo/string_view.h"
#include <catch2/catch_test_macros.hpp>
#include <cstring>

TEST_CASE("exo::String from c_string", "[string]")
{
	const char *c_s = "Im a C string.";
	auto        s   = exo::String{c_s};

	REQUIRE(s.size() == strlen(c_s));

	REQUIRE(s[0] == 'I');
	REQUIRE(s[4] == ' ');
	REQUIRE(s[5] == 'C');
	REQUIRE(s[s.size()] == '\0');

	REQUIRE(!s.is_heap_allocated());
}

TEST_CASE("exo::String push_back", "[string]")
{
	const char *c_s = "Im a C string.";
	exo::String s;

	for (const char *it = c_s; *it;) {
		s.push_back(*it);
		++it; // increment before checking size
		REQUIRE(s.size() == usize(it - c_s));
	}

	REQUIRE(s.is_heap_allocated());
	REQUIRE(exo::StringView{s} == exo::StringView{"Im a C string."});
}

TEST_CASE("exo::String move", "[string]")
{
	exo::String dyn{"Dynamically allocated because very long"};
	auto        dyn_size = dyn.size();

	REQUIRE(dyn.is_heap_allocated());

	exo::String stack{"short stack"};
	REQUIRE(!stack.is_heap_allocated());

	stack = std::move(dyn);

	// dyn was cleared because of the move
	REQUIRE(dyn.empty());
	REQUIRE(!dyn.is_heap_allocated());

	// stack stole dyn's buffer
	REQUIRE(stack.is_heap_allocated());
	REQUIRE(stack.size() == dyn_size);
}

TEST_CASE("exo::String copy", "[string]")
{
	exo::String dyn{"Dynamically allocated because very long"};

	REQUIRE(dyn.is_heap_allocated());

	exo::String stack{"short stack"};
	REQUIRE(!stack.is_heap_allocated());

	stack = dyn;

	REQUIRE(dyn.size() == stack.size());
	REQUIRE(dyn.is_heap_allocated() == stack.is_heap_allocated());

	// Check that the buffer is not shared
	REQUIRE(dyn.c_str() != stack.c_str());
	
	REQUIRE(exo::StringView{dyn} == exo::StringView{stack});
}
