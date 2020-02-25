#pragma once
#include "renderer/renderer.hpp"
#include "window.hpp"

namespace my_app
{
    class App
    {
        public:
        App();
        ~App();

        NO_COPY_NO_MOVE(App)

        void run();

        private:
        Window window;
        Renderer renderer;
    };
}    // namespace my_app
