# ImGui
add_library(imgui
  imgui/imgui.cpp
  imgui/imgui_demo.cpp
  imgui/imgui_draw.cpp
  imgui/imgui_tables.cpp
  imgui/imgui_widgets.cpp)

target_compile_definitions(imgui PUBLIC IMGUI_USER_CONFIG="imgui_config.h")
target_include_directories(imgui PRIVATE ${CMAKE_SOURCE_DIR}/exo/include/ ${CMAKE_SOURCE_DIR}/engine/src/)
set_target_properties(imgui PROPERTIES CXX_STANDARD 20)
target_compile_options(imgui PRIVATE ${THIRD_PARTY_CXX_FLAGS})

# {fmt}
add_library(fmt fmt/src/format.cc fmt/src/os.cc)
target_include_directories(fmt PUBLIC fmt/include)
target_compile_options(fmt PRIVATE ${THIRD_PARTY_CXX_FLAGS})

# meshoptimizer
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

# Vulkan Memory Allocator
add_library(vma vk_mem_alloc/vk_mem_alloc.cc)
target_link_libraries(vma Vulkan::Vulkan)
target_include_directories(vma SYSTEM PUBLIC vk_mem_alloc/)
target_compile_options(vma PRIVATE ${THIRD_PARTY_CXX_FLAGS})

# KTX-Software vulkan
add_library(libktx
    libktx/include/ktx.h
    libktx/src/basis_sgd.h
    libktx/src/basis_transcode.cpp
    libktx/src/basisu/transcoder/basisu_containers.h
    libktx/src/basisu/transcoder/basisu_containers_impl.h
    libktx/src/basisu/transcoder/basisu_file_headers.h
    libktx/src/basisu/transcoder/basisu_global_selector_cb.h
    libktx/src/basisu/transcoder/basisu_global_selector_palette.h
    libktx/src/basisu/transcoder/basisu_transcoder_internal.h
    libktx/src/basisu/transcoder/basisu_transcoder_uastc.h
    libktx/src/basisu/transcoder/basisu_transcoder.cpp
    libktx/src/basisu/transcoder/basisu_transcoder.h
    libktx/src/basisu/transcoder/basisu.h
    libktx/src/basisu/zstd/zstd.c
    libktx/src/checkheader.c
    libktx/src/dfdutils/createdfd.c
    libktx/src/dfdutils/colourspaces.c
    libktx/src/dfdutils/dfd.h
    libktx/src/dfdutils/dfd2vk.inl
    libktx/src/dfdutils/interpretdfd.c
    libktx/src/dfdutils/printdfd.c
    libktx/src/dfdutils/queries.c
    libktx/src/dfdutils/vk2dfd.c
    libktx/src/etcdec.cxx
    libktx/src/etcunpack.cxx
    libktx/src/filestream.c
    libktx/src/filestream.h
    libktx/src/formatsize.h
    libktx/src/gl_format.h
    libktx/src/hashlist.c
    libktx/src/info.c
    libktx/src/ktxint.h
    libktx/src/memstream.c
    libktx/src/memstream.h
    libktx/src/strings.c
    libktx/src/swap.c
    libktx/src/texture.c
    libktx/src/texture.h
    libktx/src/texture1.c
    libktx/src/texture1.h
    libktx/src/texture2.c
    libktx/src/texture2.h
    libktx/src/uthash.h
    libktx/src/vk_format.h
    libktx/src/vkformat_check.c
    libktx/src/vkformat_enum.h
    libktx/src/vkformat_str.c
  )


target_compile_definitions(libktx PUBLIC
  KHRONOS_STATIC
  KTX_FEATURE_WRITE=0
  BASISD_SUPPORT_KTX2_ZSTD=0
  BASISD_SUPPORT_KTX2=0
  BASISD_SUPPORT_FXT1=0
  BASISU_NO_ITERATOR_DEBUG_LEVEL)


target_include_directories(libktx SYSTEM PUBLIC libktx/include/)
target_include_directories(libktx PRIVATE
    libktx/src
    libktx/src/basisu/zstd
    libktx/src/basisu/transcoder
    libktx/other_include
    libktx/utils)

target_compile_options(libktx PRIVATE ${THIRD_PARTY_CXX_FLAGS})
target_link_libraries(libktx
  Vulkan::Vulkan
  Threads::Threads)
