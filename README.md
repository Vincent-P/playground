# My own toy renderer

This project was made to learn graphics programming using the Vulkan API.

Features:
- Physically Based Rendering
- Voxel Cone Tracing for global illumination. (Cyril Crassin, "Interactive Indirect Illumination Using Voxel Cone Tracing" https://maverick.inria.fr/Publications/2011/CNSGE11b/GIVoxels-pg2011-authors.pdf)

![PBR and VCT indirect illumination](https://media.discordapp.net/attachments/707881265751261244/755893378184642634/unknown.png?width=1280&height=720)

- State-of-the-art Procedural Sky and Atmosphere rendering. (Sébastien Hillaire, "A Scalable and Production ReadySky and Atmosphere Rendering Technique" https://sebh.github.io/publications/egsr2020.pdf)

![Atmosphere](https://media.discordapp.net/attachments/102848732738912256/776534764165398618/atmosphere.jpg?width=1280&height=720)

- Voxel debug visualization using "A Ray-Box Intersection Algorithm and Efficient Dynamic Voxel Rendering" (http://www.jcgt.org/published/0007/03/04/)

![Voxels visualization](https://media.discordapp.net/attachments/102848732738912256/776534796167806976/voxel_visualization.jpg?width=1280&height=720)

# Installation

- Install the Vulkan SDK (https://vulkan.lunarg.com/sdk/home)
- You can now build the project!

```
$ git clone https://github.com/Vincent-P/test-vulkan.git
$ cd test-vulkan
$ mkdir build
$ cd build
$ cmake -G Ninja ..
$ ninja
```

- The executable needs to be started from the build directory.

# Dependencies
- STB Image (https://github.com/nothings/stb)
- simdjson (https://github.com/simdjson/simdjson)
- Vulkan Memory Allocator (https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator)
- ImGui (https://github.com/ocornut/imgui)
- doctest (https://github.com/onqtam/doctest)
