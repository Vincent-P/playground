project('simple', 'cpp', default_options : ['default_library=static', 'buildtype=debug', 'cpp_std=c++17'])

# External dependencies
glslc = find_program('glslc', required: true)
python3 = import('python').find_installation('python3')

vulkan_dep = dependency('vulkan', required: true, method: 'system')
threads_dep = dependency('threads')

# Compiler options
cxx = meson.get_compiler('cpp')
ld_flags = []

if cxx.get_id() == 'clang'
  add_global_arguments(['-fcolor-diagnostics', '-fansi-escape-codes'], language: 'cpp') # colored output when using ninja

  if get_option('enable_sanitizer')
    add_global_arguments('-fsanitize-blacklist=' + meson.source_root() / 'asan_blacklist.txt', language : 'cpp')
    if host_machine.system() == 'windows'
      ld_flags += ['-lclang_rt.asan-x86_64', '-lclang_rt.asan_cxx-x86_64']
    endif
  endif
endif

# --- Targets

main_inc = include_directories('include')
thirdparty_inc = include_directories('third_party', is_system: true)

# - ImGui
imgui_src = files([
  'third_party/imgui/imgui.cpp',
  'third_party/imgui/imgui_demo.cpp',
  'third_party/imgui/imgui_draw.cpp',
  'third_party/imgui/imgui_tables.cpp',
  'third_party/imgui/imgui_widgets.cpp'
])
imgui = library('imgui', imgui_src, include_directories: main_inc, cpp_args: ['-DIMGUI_USER_CONFIG="imgui_config.h"'], link_args: ld_flags)

# - simdjson
simdjson = library('simdjson', 'third_party/simdjson/simdjson.cpp', link_args: ld_flags)

# - SPIRV-Reflect
spirv_reflect = library('spirv_reflect', 'third_party/SPIRV-Reflect/spirv_reflect.cc', dependencies: [vulkan_dep], link_args: ld_flags)

# - vk mem alloc
vk_mem_alloc = library('vk_mem_alloc', 'third_party/vk_mem_alloc.cc', dependencies: [vulkan_dep], link_args: ld_flags)

stb_image = library('stb_image', 'third_party/stb_image.cc', link_args: ld_flags)

# - {fmt}
fmt_inc = include_directories('third_party/fmt/include', is_system: true)
fmt_src = files([
  'third_party/fmt/src/format.cc',
  'third_party/fmt/src/os.cc'
])
fmt_lib = library('fmt', fmt_src, include_directories: fmt_inc, link_args: ld_flags)
fmt_dep = declare_dependency(link_with: fmt_lib, include_directories: fmt_inc)

# - meshoptimizer
meshopt_inc = include_directories('third_party/meshoptimizer', is_system: true)
meshopt_src = files([
  'third_party/meshoptimizer/allocator.cpp',
  'third_party/meshoptimizer/clusterizer.cpp',
  'third_party/meshoptimizer/indexcodec.cpp',
  'third_party/meshoptimizer/indexgenerator.cpp',
  'third_party/meshoptimizer/overdrawanalyzer.cpp',
  'third_party/meshoptimizer/overdrawoptimizer.cpp',
  'third_party/meshoptimizer/simplifier.cpp',
  'third_party/meshoptimizer/spatialorder.cpp',
  'third_party/meshoptimizer/stripifier.cpp',
  'third_party/meshoptimizer/vcacheanalyzer.cpp',
  'third_party/meshoptimizer/vcacheoptimizer.cpp',
  'third_party/meshoptimizer/vertexcodec.cpp',
  'third_party/meshoptimizer/vertexfilter.cpp',
  'third_party/meshoptimizer/vfetchanalyzer.cpp',
  'third_party/meshoptimizer/vfetchoptimizer.cpp'
])
meshopt_lib = library('meshopt', meshopt_src, include_directories: meshopt_inc, link_args: ld_flags)
meshopt_dep = declare_dependency(link_with: meshopt_lib, include_directories: meshopt_inc)

# - Shaders
shaders = files([
  'shaders/gui.frag',
  'shaders/gui.vert',
  'shaders/tonemap.comp.glsl',
  'shaders/average_luminance_histo.comp',
  'shaders/build_luminance_histo.comp',
  'shaders/opaque.vert',
  'shaders/opaque.frag',
  'shaders/opaque_prepass.frag',
  'shaders/path_tracer.comp.glsl',
  'shaders/hybrid.frag',
  'shaders/taa.comp.glsl',
  # 'shaders/checkerboard_floor.frag',
  # 'shaders/checkerboard_floor.vert',
  # 'shaders/depth_reduction.comp.glsl',
  # 'shaders/depth_reduction_iter.comp.glsl',
  # 'shaders/draw_call_compaction.comp.glsl',
  # 'shaders/floor_shadowmap.vert',
  # 'shaders/fullscreen_triangle.vert',
  # 'shaders/gltf.frag',
  # 'shaders/gltf.vert',
  # 'shaders/gltf_prepass.frag',
  # 'shaders/gltf_simple.frag',
  # 'shaders/gui_uint.frag',
  # 'shaders/hdr_compositing.frag',
  # 'shaders/min_lod_map.comp',
  # 'shaders/prepare_draw_indirect.comp.glsl',
  # 'shaders/shadow_bounds.comp.glsl',
  # 'shaders/shadowmap.frag',
  # 'shaders/shadowmap.vert',
  # 'shaders/sky/multiscat_lut.comp',
  # 'shaders/sky/sky_raymarch.frag',
  # 'shaders/sky/skyview_lut.frag',
  # 'shaders/sky/transmittance_lut.frag',
  # 'shaders/voxel_clear.comp',
  # 'shaders/voxel_gen_aniso_base.comp',
  # 'shaders/voxel_gen_aniso_mipmaps.comp',
  # 'shaders/voxel_inject_direct_lighting.comp',
  # 'shaders/voxel_points.frag',
  # 'shaders/voxel_points.vert',
  # 'shaders/voxelization.frag',
  # 'shaders/voxelization.geom',
  # 'shaders/voxelization.vert'
])

# Custom target to build the shaders folder in build/
shader_folder = custom_target(
  'shaders',
  command: [python3, '-c', 'import os; os.makedirs("' + meson.project_build_root() / 'shaders'+ '", exist_ok=True)'],
  output: 'shaders'
)

# Custom targets for each shaders (https://github.com/mesonbuild/meson/issues/7071#issuecomment-622599482)
shader_targets = []
foreach s : shaders
  shader_targets += custom_target(
    '@0@'.format(s).underscorify(),
    command : [glslc, '--target-env=vulkan1.2', '-std=460', '-I', meson.source_root() / 'shaders' / 'include', '@INPUT@', '-o',  meson.project_build_root() / 'shaders' / '@OUTPUT@'],
    depends: shader_folder,
    input : s,
    output : '@PLAINNAME@.spv'
  )
endforeach
shaders_dep = declare_dependency(sources: shader_targets)

# - Main target
main_src = [
  'src/asset_manager.cpp',
  'src/main.cpp',
  'src/app.cpp',
  'src/camera.cpp',
  'src/ecs.cpp',
  'src/scene.cpp',
  'src/gltf.cpp',
  'src/glb.cpp',
  'src/inputs.cpp',
  'src/tools.cpp',
  'src/ui.cpp',

  'src/base/pool.cpp',
  'src/base/vectors.cpp',
  'src/base/free_list.cpp',

  'src/platform/file_watcher.cpp',
  'src/platform/mapped_file.cpp',

  # 'src/render/bvh.cpp',
  'src/render/renderer.cpp',
  'src/render/gpu_pool.cpp',
  'src/render/ring_buffer.cpp',

  'src/render/vulkan/context.cpp',
  'src/render/vulkan/commands.cpp',
  'src/render/vulkan/descriptor_set.cpp',
  'src/render/vulkan/bindless_set.cpp',
  'src/render/vulkan/image.cpp',
  'src/render/vulkan/buffer.cpp',
  'src/render/vulkan/device.cpp',
  'src/render/vulkan/shader.cpp',
  'src/render/vulkan/surface.cpp',
  'src/render/vulkan/graphics_pipeline.cpp',
  'src/render/vulkan/compute_pipeline.cpp'
]

main_ld_flags = []
cpp_args = ['-DIMGUI_USER_CONFIG="imgui_config.h"']
main_deps = [vulkan_dep, threads_dep, shaders_dep, fmt_dep, meshopt_dep]
cpp_std = ''

if cxx.get_id() == 'msvc'
  cpp_std = 'c++latest'
elif cxx.get_id() == 'clang'
  cpp_std = 'c++2a'
elif cxx.get_id() == 'gcc'
  cpp_std = 'c++2a'
  cpp_args += '-Wno-missing-field-initializers'
else
  error('Unsupported compiler: ' + cxx.get_id())
endif


# Platform specifics
if host_machine.system() == 'windows'
  main_src += [
    'src/platform/utils_win32.cpp',
    'src/platform/window_win32.cpp',
    'src/platform/file_dialog_win32.cpp',
  ]
  main_ld_flags += '-limm32' # Doesnt work with cl?

  cpp_args += [
    '-DNOMINMAX',
    '-DNOCOMM',
    '-DVC_EXTRALEAN',
    '-DWIN32_LEAN_AND_MEAN',
    '-DUNICODE',
    '-DVK_USE_PLATFORM_WIN32_KHR'
  ]
elif host_machine.system() == 'linux'
  # On linux, the standard parallel execution depends on Intel's tbb
  main_ld_flags += ['-ldl', '-ltbb']

  if get_option('enable_wayland')
      main_src += 'src/platform/window_wayland.cpp'
      cpp_args += '-DVK_USE_PLATFORM_WAYLAND_KHR'
  else
      main_src += 'src/platform/window_xcb.cpp'
      cpp_args += '-DVK_USE_PLATFORM_XCB_KHR'

      main_deps += [
        dependency('xcb', required: true),
        dependency('xkbcommon', required: true),
        dependency('xkbcommon-x11', required: true),
      ]
  endif
else
  error('Unsupported platform.')
endif

if get_option('enable_vma_debug')
  cpp_args += [
    '-DVMA_DEBUG_INITIALIZE_ALLOCATIONS=1',
    '-DVMA_DEBUG_MARGIN=16',
    '-DVMA_DEBUG_DETECT_CORRUPTION=1'
  ]
endif

if get_option('enable_renderdoc')
  cpp_args += '-DENABLE_RENDERDOC'
endif

if get_option('enable_doctest')
  cpp_args += '-DENABLE_DOCTEST'
endif

executable('test-vulkan', main_src,
  include_directories: [main_inc, thirdparty_inc],
  dependencies: main_deps,
  link_with: [imgui, spirv_reflect, simdjson, vk_mem_alloc, stb_image],
  override_options : ['cpp_std=' + cpp_std, 'warning_level=2', 'werror=true', ],
  cpp_args: cpp_args,
  link_args: ld_flags + main_ld_flags,
)
