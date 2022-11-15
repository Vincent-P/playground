#include "exo/string.h"
#include "exo/string_view.h"
#include "helpers.h"
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

	auto s2 = exo::String{c_s, 2};

	REQUIRE(s2.size() == 2);
	REQUIRE(s2[0] == 'I');
	REQUIRE(s2[1] == 'm');
	REQUIRE(s2[2] == '\0');
	REQUIRE(!s2.is_heap_allocated());
}

TEST_CASE("exo::StringView from c_string", "[string]")
{
	const char *c_s = "Im a C string.";
	auto        s   = exo::StringView{c_s};

	REQUIRE(s.size() == strlen(c_s));
	REQUIRE(s[0] == 'I');
	REQUIRE(s[4] == ' ');
	REQUIRE(s[5] == 'C');

	auto s2 = exo::StringView{c_s, 2};

	REQUIRE(s2.size() == 2);
	REQUIRE(s2[0] == 'I');
	REQUIRE(s2[1] == 'm');
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
	REQUIRE(dyn == stack);
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

TEST_CASE("exo::String clear", "[string]")
{
	exo::StringView short_str_view = "short string";
	exo::StringView long_str_view  = "short string string very long";

	exo::String short_str = short_str_view;
	exo::String long_str  = long_str_view;

	short_str.clear();
	long_str.clear();

	REQUIRE(short_str.is_empty());
	REQUIRE(short_str.capacity() == exo::String::SSBO_CAPACITY);

	REQUIRE(long_str.is_empty());
	REQUIRE(long_str.capacity() == long_str_view.size() + 1);
}

TEST_CASE("exo::String resize", "[string]")
{
	exo::String s;

	// Resizing less than SSBO_CAPACITY does not allocate
	exo::StringView short_str = "short string";
	s.resize(short_str.size());
	std::memcpy(s.data(), short_str.data(), short_str.size());

	REQUIRE(short_str.size() < exo::String::SSBO_CAPACITY);
	REQUIRE(s.size() == short_str.size());
	REQUIRE(s.capacity() == exo::String::SSBO_CAPACITY);
	REQUIRE(s == short_str);

	// Resizing more than SSBO_CAPACITY will allocate
	exo::StringView long_str = "short string string very long";
	s.resize(long_str.size());
	std::memcpy(s.data(), long_str.data(), long_str.size());

	REQUIRE(long_str.size() > exo::String::SSBO_CAPACITY);
	REQUIRE(s.size() == long_str.size());
	REQUIRE(s.capacity() == long_str.size() + 1);
	REQUIRE(s == long_str);
}

TEST_CASE("exo::String reserve", "[string]")
{
	exo::StringView long_str = "Dynamically allocated because very long";

	exo::String s;

	// Reserving less than the SSBO_CAPACITY does not change the capacity.
	s.reserve(4);
	REQUIRE(s.size() == 0);
	REQUIRE(s.capacity() == exo::String::SSBO_CAPACITY);

	// Reserving more than the SSBO_CAPACITY will allocate and use the capacity
	s.reserve(long_str.size() + 1);
	REQUIRE(s.size() == 0);
	REQUIRE(s.capacity() == long_str.size() + 1);
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
	REQUIRE(s.capacity() == 2 * exo::String::SSBO_CAPACITY);
	REQUIRE(exo::StringView{s} == exo::StringView{"Im a C string."});
}

TEST_CASE("exo::String concat", "[string]")
{
	exo::String dyn{"Dynamically allocated because very long"};
	exo::String stack{" short stack"};

	auto concat = dyn + stack;

	REQUIRE(concat.size() == exo::StringView{"Dynamically allocated because very long short stack"}.size());
	REQUIRE(concat == exo::StringView{"Dynamically allocated because very long short stack"});
	REQUIRE(concat == "Dynamically allocated because very long short stack");
}

TEST_CASE("exo::String operator==", "[string]")
{
	const char      c_string[]     = "I'm a big C string.";
	exo::String     different      = "different";
	exo::String     same           = c_string;
	exo::StringView different_view = "different";
	exo::StringView same_view      = "I'm a big C string.";

	// String == String
	REQUIRE(!(different == same));
	REQUIRE(!(same == different));

	// String == StringView
	REQUIRE(different == different_view);
	REQUIRE(!(different == same_view));
	REQUIRE(same_view == same);
	REQUIRE(!(same_view == different_view));

	// const char[] == String
	REQUIRE(c_string == same);
	REQUIRE(!(different == c_string));
}
