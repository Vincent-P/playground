#include "app.h"
#include <exo/types.h>
#include <exo/logger.h>

#include "glb.h"
#include "asset_manager.h"

#if defined (ENABLE_DOCTEST)
#define DOCTEST_CONFIG_COLORS_NONE
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest.h>
#endif

int main(int argc, char **argv)
{
    #if defined (ENABLE_DOCTEST)
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
    #else
    UNUSED(argc);
    UNUSED(argv);
    #endif

    #if 0
    auto scene = glb::load_file("C:/Users/vince/Documents/code/test-vulkan/models/Sponza.glb");
    UNUSED(scene);
    #endif

    App app;
    app.run();
    return 0;
}
