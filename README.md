# Vulkan glTF renderer

This project was made to learn the graphics programming using the Vulkan API.

Features:
- Physically Based Rendering
- Voxel Cone Tracing for global illumination. (Cyril Crassin, "Interactive Indirect Illumination Using Voxel Cone Tracing" https://maverick.inria.fr/Publications/2011/CNSGE11b/GIVoxels-pg2011-authors.pdf)
- State-of-the-art Procedural Sky and Atmosphere rendering. (Sébastien Hillaire, "A Scalable and Production ReadySky and Atmosphere Rendering Technique" https://sebh.github.io/publications/egsr2020.pdf)

![Screen shot of Sponza, a scene often used in tech demos](https://cdn.discordapp.com/attachments/102848732738912256/755721885685252226/unknown.png)

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
