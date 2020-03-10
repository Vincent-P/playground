#include "tools.hpp"
#include "gtest/gtest.h"

using namespace my_app;

TEST(ArenaTest, Size)
{
    Arena<uint> numbers;
    auto h1 = numbers.add(1);

    EXPECT_EQ(numbers.get_size(), 1);

    numbers.remove(h1);

    EXPECT_EQ(numbers.get_size(), 0);
}
