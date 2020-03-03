#include <iostream>

#include "app.hpp"
#include "gltf.hpp"

int main(int /*argc*/, char ** /*argv*/)
{
    try {
	my_app::App app;
	app.run();
	return 0;
    }
    catch (const std::exception &exception) {
	std::cerr << "Fatal exception: " << exception.what() << std::endl;
    }
}
