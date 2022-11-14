#include "exo/path.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("exo::Path concatenation", "[path]")
{
	auto path = exo::Path::from_string("a");

	path = exo::Path::join(path, "b");
	REQUIRE(path.str == "a/b");

	path = exo::Path::join(path, "./c");
	REQUIRE(path.str == "a/b/c");

	path = exo::Path::join(path, ".\\d");
	REQUIRE(path.str == "a/b/c/d");

	path = exo::Path::join(path, "\\e");
	REQUIRE(path.str == "/e");

	path = exo::Path::join(path, "/f");
	REQUIRE(path.str == "/f");
}

TEST_CASE("exo::Path from string", "[path]")
{
	auto windows_path = exo::Path::from_string("C:/Windows\\System/Users\\test");
	REQUIRE(windows_path.str == "C:/Windows/System/Users/test");

	auto linux_path = exo::Path::from_string("/root/system/windows/users");
	REQUIRE(linux_path.str == "/root/system/windows/users");
}

TEST_CASE("exo::Path filename", "[path]")
{
	auto test_path     = exo::Path::from_string("test.txt");
	auto test_filename = exo::String{test_path.filename()}; // Catch2's REQUIRE wants a exo::String
	REQUIRE(test_filename == "test.txt");

	test_path     = exo::Path::from_string("chemin/test.txt");
	test_filename = exo::String{test_path.filename()}; // Catch2's REQUIRE wants a exo::String
	REQUIRE(test_filename == "test.txt");
}

TEST_CASE("exo::Path replace filename", "[path]")
{
	auto test_path  = exo::Path::from_string("C:/Windows/System/Users/test");
	auto other_path = exo::Path::replace_filename(test_path, "other");
	REQUIRE(other_path.str == "C:/Windows/System/Users/other");

	test_path  = exo::Path::from_string("C:/Windows/System/Users/test.txt");
	other_path = exo::Path::replace_filename(test_path, "other.gltf");
	REQUIRE(other_path.str == "C:/Windows/System/Users/other.gltf");
}
