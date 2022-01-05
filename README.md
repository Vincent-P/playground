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

The only external dependency needed is the `glslc` shader compiler, you can download the latest binary build here: https://github.com/google/shaderc/blob/main/downloads.md
`glslc` should be in the PATH.

There are two presets: `default` which uses the Ninja build system with clang; and `msvc` which creates a VS2022 solution.

```
$ git clone https://github.com/Vincent-P/test-vulkan.git
$ cd test-vulkan
$ cmake --preset default
$ cmake --build --preset default
```

# Dependencies
- Dear ImGui, Bloat-free Graphical User interface for C++ with minimal dependencies (https://github.com/ocornut/imgui)
- meshoptimizer, Mesh optimization library that makes meshes smaller and faster to render (https://github.com/zeux/meshoptimizer)
- Vulkan Headers, Vulkan Header files and API registry (https://github.com/KhronosGroup/Vulkan-Headers)
- volk, Meta loader for Vulkan API (https://github.com/zeux/volk)
- Vulkan Memory Allocator, Easy to integrate Vulkan memory allocation library (https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator)
- Tracy Profiler, A real time, nanosecond resolution, remote telemetry, hybrid frame and sampling profiler for games and other applications (https://github.com/wolfpld/tracy)
- {fmt}, A modern formatting library (https://github.com/fmtlib/fmt)
- LEAF, A lightweight error handling library for C++11 (https://github.com/boostorg/leaf)
- parallel-hashmap, A family of header-only, very fast and memory-friendly hashmap and btree containers (https://github.com/greg7mdp/parallel-hashmap)
- zlib-ng, zlib replacement with optimizations for "next generation" systems (https://github.com/zlib-ng/zlib-ng)
- libktx, a small library of functions for writing and reading KTX files (https://github.com/KhronosGroup/KTX-Software)
- libspng, Simple, modern libpng alternative (https://github.com/randy408/libspng)
- RapidJSON, A fast JSON parser/generator for C++ with both SAX/DOM style API (https://github.com/Tencent/rapidjson)
- xxhash, Extremely fast non-cryptographic hash algorithm (https://github.com/Cyan4973/xxHash)
- Meow hash, an extremely fast level 1 hash (https://github.com/cmuratori/meow_hash)
