add_library(imgui
  imgui/imgui.cpp
  imgui/imgui_demo.cpp
  imgui/imgui_draw.cpp
  imgui/imgui_tables.cpp
  imgui/imgui_widgets.cpp)

target_compile_definitions(imgui PUBLIC IMGUI_USER_CONFIG="imgui_config.h")
target_include_directories(imgui PRIVATE ${CMAKE_SOURCE_DIR}/engine/src/)
set_target_properties(imgui PROPERTIES CXX_STANDARD 20)
target_compile_options(imgui PRIVATE ${THIRD_PARTY_CXX_FLAGS})

add_library(simdjson simdjson/simdjson.cpp)
target_compile_options(simdjson PRIVATE ${THIRD_PARTY_CXX_FLAGS})

add_library(fmt fmt/src/format.cc fmt/src/os.cc)
target_include_directories(fmt PUBLIC fmt/include)
target_compile_options(fmt PRIVATE ${THIRD_PARTY_CXX_FLAGS})

add_library(stb_image stb_image.cc)
target_compile_options(stb_image PRIVATE ${THIRD_PARTY_CXX_FLAGS})

add_library(meshopt
  meshoptimizer/allocator.cpp
  meshoptimizer/clusterizer.cpp
  meshoptimizer/indexcodec.cpp
  meshoptimizer/indexgenerator.cpp
  meshoptimizer/overdrawanalyzer.cpp
  meshoptimizer/overdrawoptimizer.cpp
  meshoptimizer/simplifier.cpp
  meshoptimizer/spatialorder.cpp
  meshoptimizer/stripifier.cpp
  meshoptimizer/vcacheanalyzer.cpp
  meshoptimizer/vcacheoptimizer.cpp
  meshoptimizer/vertexcodec.cpp
  meshoptimizer/vertexfilter.cpp
  meshoptimizer/vfetchanalyzer.cpp
  meshoptimizer/vfetchoptimizer.cpp)

target_include_directories(meshopt SYSTEM PUBLIC meshoptimizer/)
target_compile_options(meshopt PRIVATE ${THIRD_PARTY_CXX_FLAGS})

add_library(vma vk_mem_alloc.cc)
target_link_libraries(vma Vulkan::Vulkan)
target_compile_options(vma PRIVATE ${THIRD_PARTY_CXX_FLAGS})
