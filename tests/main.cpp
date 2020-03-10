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


TEST(ArenaTest, ForLoop)
{
    Arena<uint> numbers;
    auto h1 = numbers.add(1);

    usize nb = 0;
    for (auto number : numbers)
    {
        EXPECT_EQ(number, 1);
        nb++;
    }

    EXPECT_EQ(nb, 1);

    numbers.remove(h1);

    nb = 0;
    for (auto number : numbers)
    {
        nb++;
    }

    EXPECT_EQ(nb, 0);
}
