#include "base/pool.hpp"

#include <doctest.h>

namespace test
{
    TEST_CASE("Handles")
    {
        auto h1 = Handle<int>(0);
        auto h2 = Handle<int>(1);
        auto h3 = h1;

        CHECK(h1.value() == 0);
        CHECK(h2.value() == 1);
        CHECK(h1.hash() == h3.hash());
    }

}
