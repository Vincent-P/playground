# Vulkan glTF renderer

This project was made to learn the Vulkan API, and more specifically the C++ bindings of Vulkan.

It can render basic glTF 2 models with an incomplete PBR (Physically Based Rendering) implementation.

![Screen shot of Sponza, a scene often used in tech demos](https://cdn.discordapp.com/attachments/102848732738912256/617819661182173214/unknown.png)

# Installation

- Install the Vulkan SDK (https://vulkan.lunarg.com/sdk/home)

- Test vulkan uses `vckpg` to manage its dependencies, so first install it:

```
> git clone https://github.com/Microsoft/vcpkg.git
> cd vcpkg

PS> .\bootstrap-vcpkg.bat
Linux:~/$ ./bootstrap-vcpkg.sh

> cd ..
```

- And set the VCPKG_ROOT environment variable to the vcpkg folder.

- Install the dependencies, replace `x64-windows` by `x64-linux` or `x64-osx` depending on your platform:

```
vcpkg install glfw3 glm nlohmann-json stb vulkan-memory-allocator --triplet x64-windows
```

- You can now build the project!

```
git clone https://github.com/Vincent-P/test-vulkan.git
cd test-vulkan

mkdir build
cd build

cmake -G Ninja ..
ninja
```

- The executable needs to be started from the root of the project.
