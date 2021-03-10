#include "app.hpp"
#include "base/types.hpp"

#define DOCTEST_CONFIG_COLORS_NONE
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest.h>

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    #if 0
    doctest::Context context;
    context.applyCommandLine(argc, argv);
    int res = context.run(); // run
    if (context.shouldExit())
    {               // important - query flags (and --exit) rely on the user doing this
        return res; // propagate the result of the tests
    }

    if (res)
    {
        return res;
    }
    #endif

    App app;
    app.run();
    return 0;
}
