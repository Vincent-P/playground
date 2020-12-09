#include "app.hpp"
#include "base/types.hpp"
#include "gltf.hpp"

#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest.h>

int main(int argc, char **argv)
{
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

    my_app::App app;
    app.run();
    return 0;
}
