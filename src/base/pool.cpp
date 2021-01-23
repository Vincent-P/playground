#include "base/pool.hpp"

#include <doctest.h>

namespace test
{
    TEST_CASE("Handles")
    {
        auto h1 = Handle<int>(0);
        auto h2 = Handle<int>(1);

        // Value returns the index of the handle
        CHECK(h1.value() == 0);
        CHECK(h2.value() == 1);

        auto h3 = h1;
        // Copy constructor does not increment the generation field!
        CHECK(h1.hash() == h3.hash());

        Handle<int> h4;
        // Default constructor returns an invalid handle
        CHECK(!h4.is_valid());

        // Assignement operator works!
        h4 = h3;
        CHECK(h4.is_valid());
        CHECK(h4.hash() == h3.hash());
    }

}
