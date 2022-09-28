find_package(Threads REQUIRED)

# {fmt}
add_library(fmt fmt/src/format.cc fmt/src/os.cc)
setup_thirdparty_target(fmt)
target_include_directories(fmt SYSTEM PUBLIC fmt/include)

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
setup_thirdparty_target(meshopt)
target_include_directories(meshopt SYSTEM PUBLIC meshoptimizer/)

# Vulkan Headers
add_library(vulkan_headers INTERFACE)
target_include_directories(vulkan_headers SYSTEM INTERFACE vulkan_headers/include)

# Vulkan Memory Allocator
add_library(vma vk_mem_alloc/vk_mem_alloc.cc)
setup_thirdparty_target(vma)
target_include_directories(vma SYSTEM PUBLIC vk_mem_alloc/)
target_compile_definitions(vma PRIVATE VMA_STATIC_VULKAN_FUNCTIONS=0)
target_link_libraries(vma PRIVATE vulkan_headers)

#volk
add_library(volk volk/volk.c)
setup_thirdparty_target(volk)
target_include_directories(volk SYSTEM PUBLIC volk/)
target_link_libraries(volk PUBLIC vulkan_headers)
target_compile_definitions(volk PUBLIC
  $<$<BOOL:${WIN32}>:VK_USE_PLATFORM_WIN32_KHR>
  $<$<BOOL:${UNIX}>:VK_USE_PLATFORM_XCB_KHR>
  VK_NO_PROTOTYPES
)

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
setup_thirdparty_target(libktx)
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
target_link_libraries(libktx
  volk
  Threads::Threads)


# zlib-ng
add_library(zlib_ng
    zlib_ng/adler32.c
    zlib_ng/chunkset.c
    zlib_ng/compare258.c
    zlib_ng/compress.c
    zlib_ng/crc32.c
    zlib_ng/crc32_comb.c
    zlib_ng/deflate.c
    zlib_ng/deflate_fast.c
    zlib_ng/deflate_medium.c
    zlib_ng/deflate_quick.c
    zlib_ng/deflate_slow.c
    zlib_ng/functable.c
    zlib_ng/infback.c
    zlib_ng/inffast.c
    zlib_ng/inflate.c
    zlib_ng/inftrees.c
    zlib_ng/insert_string.c
    zlib_ng/trees.c
    zlib_ng/uncompr.c
    zlib_ng/zutil.c
    )
setup_thirdparty_target(zlib_ng)
target_include_directories(zlib_ng SYSTEM PUBLIC zlib_ng/)
target_compile_definitions(zlib_ng PRIVATE ZLIB_COMPAT)

#libspng
add_library(libspng libspng/spng.c)
setup_thirdparty_target(libspng)
target_link_libraries(libspng zlib_ng)
target_include_directories(libspng SYSTEM PUBLIC libspng/)
target_compile_definitions(libspng PUBLIC SPNG_STATIC)

#Tracy profiler
add_library(tracy tracy/TracyClient.cpp)
setup_thirdparty_target(tracy)
target_include_directories(tracy SYSTEM PUBLIC tracy/)
target_compile_definitions(tracy PRIVATE TRACY_ENABLE TRACY_HAS_CALLSTACK)
target_compile_definitions(tracy PUBLIC TRACY_FIBERS)
target_link_libraries(tracy ${CMAKE_DL_LIBS})

#parallel hashmap
add_library(parallel_hashmap INTERFACE)
target_include_directories(parallel_hashmap SYSTEM INTERFACE parallel_hashmap/)

#xxhash
add_library(xxhash INTERFACE)
target_compile_definitions(xxhash INTERFACE XXH_INLINE_ALL XXH_PRIVATE_API)
target_include_directories(xxhash SYSTEM INTERFACE xxhash/)

#rapidjson
add_library(rapidjson INTERFACE)
target_compile_definitions(rapidjson INTERFACE RAPIDJSON_ASSERT=ASSERT)
target_include_directories(rapidjson SYSTEM INTERFACE rapidjson/)

#meow hash
add_library(meow_hash INTERFACE)
target_include_directories(meow_hash SYSTEM INTERFACE meow_hash/)


#freetype without harfbuzz
# read ftconfig
if (UNIX)
  check_include_file("unistd.h" HAVE_UNISTD_H)
  check_include_file("fcntl.h" HAVE_FCNTL_H)

  file(READ "${CMAKE_CURRENT_SOURCE_DIR}/freetype/builds/unix/ftconfig.h.in" FTCONFIG_H)
  if (HAVE_UNISTD_H)
    string(REGEX REPLACE
      "#undef +(HAVE_UNISTD_H)" "#define \\1 1"
      FTCONFIG_H "${FTCONFIG_H}")
  endif ()
  if (HAVE_FCNTL_H)
    string(REGEX REPLACE
      "#undef +(HAVE_FCNTL_H)" "#define \\1 1"
      FTCONFIG_H "${FTCONFIG_H}")
  endif ()
else ()
  file(READ "${CMAKE_CURRENT_SOURCE_DIR}/freetype/include/freetype/config/ftconfig.h" FTCONFIG_H)
endif ()
# set ftconfig build output
set(FTCONFIG_H_NAME "${CMAKE_CURRENT_BINARY_DIR}/freetype_no_hb/include/freetype/config/ftconfig.h")
#write if needed
if (EXISTS "${FTCONFIG_H_NAME}")
  file(READ "${FTCONFIG_H_NAME}" ORIGINAL_FTCONFIG_H)
else ()
  set(ORIGINAL_FTCONFIG_H "")
endif ()
if (NOT (ORIGINAL_FTCONFIG_H STREQUAL FTCONFIG_H))
  file(WRITE "${FTCONFIG_H_NAME}" "${FTCONFIG_H}")
endif ()

# Create the options file
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/freetype/include/freetype/config/ftoption.h" FTOPTION_H)
#set ftoption build output
set(FTOPTION_H_NAME "${CMAKE_CURRENT_BINARY_DIR}/freetype_no_hb/include/freetype/config/ftoption.h")
#write if needed
if (EXISTS "${FTOPTION_H_NAME}")
  file(READ "${FTOPTION_H_NAME}" ORIGINAL_FTOPTION_H)
else ()
  set(ORIGINAL_FTOPTION_H "")
endif ()
if (NOT (ORIGINAL_FTOPTION_H STREQUAL FTOPTION_H))
  file(WRITE "${FTOPTION_H_NAME}" "${FTOPTION_H}")
endif ()

file(GLOB FREETYPE_PUBLIC_HEADERS "freetype/include/ft2build.h" "freetype/include/freetype/*.h")
file(GLOB FREETYPE_PUBLIC_CONFIG_HEADERS "freetype/include/freetype/config/*.h")
file(GLOB FREETYPE_PRIVATE_HEADERS "freetype/include/freetype/internal/*.h")

set(FREETYPE_BASE_SRCS
  freetype/src/autofit/autofit.c
  freetype/src/base/ftbase.c
  freetype/src/base/ftbbox.c
  freetype/src/base/ftbdf.c
  freetype/src/base/ftbitmap.c
  freetype/src/base/ftcid.c
  freetype/src/base/ftfstype.c
  freetype/src/base/ftgasp.c
  freetype/src/base/ftglyph.c
  freetype/src/base/ftgxval.c
  freetype/src/base/ftinit.c
  freetype/src/base/ftmm.c
  freetype/src/base/ftotval.c
  freetype/src/base/ftpatent.c
  freetype/src/base/ftpfr.c
  freetype/src/base/ftstroke.c
  freetype/src/base/ftsynth.c
  freetype/src/base/fttype1.c
  freetype/src/base/ftwinfnt.c
  freetype/src/bdf/bdf.c
  freetype/src/bzip2/ftbzip2.c
  freetype/src/cache/ftcache.c
  freetype/src/cff/cff.c
  freetype/src/cid/type1cid.c
  freetype/src/gzip/ftgzip.c
  freetype/src/lzw/ftlzw.c
  freetype/src/pcf/pcf.c
  freetype/src/pfr/pfr.c
  freetype/src/psaux/psaux.c
  freetype/src/pshinter/pshinter.c
  freetype/src/psnames/psnames.c
  freetype/src/raster/raster.c
  freetype/src/sdf/sdf.c
  freetype/src/sfnt/sfnt.c
  freetype/src/smooth/smooth.c
  freetype/src/truetype/truetype.c
  freetype/src/type1/type1.c
  freetype/src/type42/type42.c
  freetype/src/winfonts/winfnt.c
)

# if (UNIX)
#   list(APPEND FREETYPE_BASE_SRCS freetype/builds/unix/ftsystem.c)
# elseif (WIN32)
#   list(APPEND FREETYPE_BASE_SRCS freetype/builds/windows/ftsystem.c)
#   list(APPEND FREETYPE_BASE_SRCS freetype/builds/windows/ftdebug.c)
# else ()
#   list(APPEND FREETYPE_BASE_SRCS freetype/src/base/ftsystem.c)
#   list(APPEND FREETYPE_BASE_SRCS freetype/src/base/ftdebug.c)
# endif ()
list(APPEND FREETYPE_BASE_SRCS freetype/src/base/ftsystem.c)
list(APPEND FREETYPE_BASE_SRCS freetype/src/base/ftdebug.c)

add_library(freetype_no_hb
  ${FREETYPE_PUBLIC_HEADERS}
  ${FREETYPE_PUBLIC_CONFIG_HEADERS}
  ${FREETYPE_PRIVATE_HEADERS}
  ${FREETYPE_BASE_SRCS}
)
setup_thirdparty_target(freetype_no_hb)

set_target_properties(freetype_no_hb PROPERTIES C_VISIBILITY_PRESET hidden)

target_compile_definitions(freetype_no_hb PRIVATE FT2_BUILD_LIBRARY)
if (WIN32)
  target_compile_definitions(freetype_no_hb PRIVATE _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS)
endif ()

target_include_directories(
  freetype_no_hb
    PUBLIC
      $<INSTALL_INTERFACE:freetype/include/freetype2>
      $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/freetype_no_hb/include>
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/freetype/include>
    PRIVATE
      ${CMAKE_CURRENT_BINARY_DIR}/freetype_no_hb/include
      ${CMAKE_CURRENT_SOURCE_DIR}/freetype/include

      # Make <ftconfig.h> available for builds/unix/ftsystem.c.
      ${CMAKE_CURRENT_BINARY_DIR}/freetype_no_hb/include/freetype/config
)


#harfbuzz
set(HARFBUZZ_SOURCES
  harfbuzz/src/hb-aat-layout.cc
  harfbuzz/src/hb-aat-map.cc
  harfbuzz/src/hb-blob.cc
  harfbuzz/src/hb-buffer-serialize.cc
  harfbuzz/src/hb-buffer.cc
  harfbuzz/src/hb-common.cc
  harfbuzz/src/hb-draw.cc
  harfbuzz/src/hb-face.cc
  harfbuzz/src/hb-fallback-shape.cc
  harfbuzz/src/hb-font.cc
  harfbuzz/src/hb-map.cc
  harfbuzz/src/hb-ms-feature-ranges.cc
  harfbuzz/src/hb-number.cc
  harfbuzz/src/hb-ot-cff1-table.cc
  harfbuzz/src/hb-ot-cff2-table.cc
  harfbuzz/src/hb-ot-color.cc
  harfbuzz/src/hb-ot-face.cc
  harfbuzz/src/hb-ot-font.cc
  harfbuzz/src/hb-ot-layout.cc
  harfbuzz/src/hb-ot-map.cc
  harfbuzz/src/hb-ot-math.cc
  harfbuzz/src/hb-ot-meta.cc
  harfbuzz/src/hb-ot-metrics.cc
  harfbuzz/src/hb-ot-name.cc
  harfbuzz/src/hb-ot-shape-complex-arabic.cc
  harfbuzz/src/hb-ot-shape-complex-default.cc
  harfbuzz/src/hb-ot-shape-complex-hangul.cc
  harfbuzz/src/hb-ot-shape-complex-hebrew.cc
  harfbuzz/src/hb-ot-shape-complex-indic-table.cc
  harfbuzz/src/hb-ot-shape-complex-indic.cc
  harfbuzz/src/hb-ot-shape-complex-khmer.cc
  harfbuzz/src/hb-ot-shape-complex-myanmar.cc
  harfbuzz/src/hb-ot-shape-complex-syllabic.cc
  harfbuzz/src/hb-ot-shape-complex-thai.cc
  harfbuzz/src/hb-ot-shape-complex-use.cc
  harfbuzz/src/hb-ot-shape-complex-vowel-constraints.cc
  harfbuzz/src/hb-ot-shape-fallback.cc
  harfbuzz/src/hb-ot-shape-normalize.cc
  harfbuzz/src/hb-ot-shape.cc
  harfbuzz/src/hb-ot-tag.cc
  harfbuzz/src/hb-ot-var.cc
  harfbuzz/src/hb-set.cc
  harfbuzz/src/hb-shape-plan.cc
  harfbuzz/src/hb-shape.cc
  harfbuzz/src/hb-shaper.cc
  harfbuzz/src/hb-static.cc
  harfbuzz/src/hb-style.cc
  harfbuzz/src/hb-ucd.cc
  harfbuzz/src/hb-unicode.cc
  harfbuzz/src/hb-ft.cc
  )

add_library(harfbuzz ${HARFBUZZ_SOURCES})
setup_thirdparty_target(harfbuzz)
target_link_libraries(harfbuzz PRIVATE freetype_no_hb)
target_include_directories(harfbuzz SYSTEM PUBLIC harfbuzz/src)
target_compile_definitions(harfbuzz PRIVATE HAVE_FREETYPE)

#freetype with harfbuzz
set(FTCONFIG_H_NAME "${CMAKE_CURRENT_BINARY_DIR}/freetype/include/freetype/config/ftconfig.h")
if (EXISTS "${FTCONFIG_H_NAME}")
  file(READ "${FTCONFIG_H_NAME}" ORIGINAL_FTCONFIG_H)
else ()
  set(ORIGINAL_FTCONFIG_H "")
endif ()
if (NOT (ORIGINAL_FTCONFIG_H STREQUAL FTCONFIG_H))
  file(WRITE "${FTCONFIG_H_NAME}" "${FTCONFIG_H}")
endif ()

#enable harfbuzz in ftoption
string(REGEX REPLACE
"/\\* +(#define +FT_CONFIG_OPTION_USE_HARFBUZZ) +\\*/" "\\1"
FTOPTION_H "${FTOPTION_H}")
#write in build output
set(FTOPTION_H_NAME "${CMAKE_CURRENT_BINARY_DIR}/freetype/include/freetype/config/ftoption.h")
if (EXISTS "${FTOPTION_H_NAME}")
  file(READ "${FTOPTION_H_NAME}" ORIGINAL_FTOPTION_H)
else ()
  set(ORIGINAL_FTOPTION_H "")
endif ()
if (NOT (ORIGINAL_FTOPTION_H STREQUAL FTOPTION_H))
  file(WRITE "${FTOPTION_H_NAME}" "${FTOPTION_H}")
endif ()

add_library(freetype
  ${FREETYPE_PUBLIC_HEADERS}
  ${FREETYPE_PUBLIC_CONFIG_HEADERS}
  ${FREETYPE_PRIVATE_HEADERS}
  ${FREETYPE_BASE_SRCS}
)
setup_thirdparty_target(freetype)
set_target_properties(freetype PROPERTIES C_VISIBILITY_PRESET hidden)
target_compile_definitions(freetype PRIVATE FT2_BUILD_LIBRARY)
if (WIN32)
  target_compile_definitions(freetype PRIVATE _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS)
endif ()
target_include_directories(
  freetype
    PUBLIC
      $<INSTALL_INTERFACE:freetype/include/freetype2>
      $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/freetype/include>
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/freetype/include>
    PRIVATE
      ${CMAKE_CURRENT_BINARY_DIR}/freetype/include
      ${CMAKE_CURRENT_SOURCE_DIR}/freetype/include

      # Make <ftconfig.h> available for builds/unix/ftsystem.c.
      ${CMAKE_CURRENT_BINARY_DIR}/freetype/include/freetype/config
)
target_link_libraries(freetype PRIVATE harfbuzz)

# sokol
add_library(sokol INTERFACE)
target_include_directories(sokol SYSTEM INTERFACE sokol)
