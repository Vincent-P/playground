project('simple', 'cpp', default_options: ['default_library=static', 'buildtype=debug', 'b_vscrt=mtd'])

vulkan = dependency('vulkan')
glslc = find_program('glslc')

main_inc = include_directories('include')
thirdparty_inc = include_directories('third_party')
fmt_inc = include_directories('third_party/fmt/include')

# ImGui
imgui_src = files([
  'third_party/imgui/imgui.cpp',
  'third_party/imgui/imgui_demo.cpp',
  'third_party/imgui/imgui_draw.cpp',
  'third_party/imgui/imgui_tables.cpp',
  'third_party/imgui/imgui_widgets.cpp'
])
imgui = library('imgui', imgui_src, include_directories: main_inc)

# simdjson
simdjson = library('simdjson', 'third_party/simdjson/simdjson.cpp')

# SPIRV-Reflect
spirv_reflect = library('spirv_reflect', 'third_party/SPIRV-Reflect/spirv_reflect.cc', dependencies: [vulkan])

# {fmt}
fmt_src = files([
  'third_party/fmt/src/format.cc',
  'third_party/fmt/src/os.cc'
])
fmt = library('fmt', fmt_src, include_directories: fmt_inc)

# Main source files
main_src = [
  'src/app.cpp',
  'src/base/pool.cpp',
  'src/base/vectors.cpp',
  'src/camera.cpp',
  'src/ecs.cpp',
  'src/file_watcher.cpp',
  'src/gltf.cpp',
  'src/inputs.cpp',
  'src/main.cpp',
  'src/platform/window_win32.cpp',
  # 'src/platform/window_xcb.cpp',
  'src/render/cascades_bounds_pass.cpp',
  'src/render/grid_pass.cpp',
  'src/render/hl_api.cpp',
  'src/render/hl_api_draw.cpp',
  'src/render/hl_api_resources.cpp',
  'src/render/imgui_pass.cpp',
  'src/render/luminance_pass.cpp',
  'src/render/render_graph.cpp',
  'src/render/renderer.cpp',
  'src/render/sky_pass.cpp',
  'src/render/tonemap_pass.cpp',
  'src/render/vlk_context.cpp',
  'src/timer.cpp',
  'src/tools.cpp',
  'src/ui.cpp'
]

shaders = files([
  'shaders/average_luminance_histo.comp',
  'shaders/build_luminance_histo.comp',
  'shaders/checkerboard_floor.frag',
  'shaders/checkerboard_floor.vert',
  'shaders/depth_reduction.comp.glsl',
  'shaders/depth_reduction_iter.comp.glsl',
  'shaders/draw_call_compaction.comp.glsl',
  'shaders/floor_shadowmap.vert',
  'shaders/fullscreen_triangle.vert',
  # 'shaders/gltf.frag',
  'shaders/gltf.vert',
  'shaders/gltf_prepass.frag',
  'shaders/gltf_simple.frag',
  'shaders/gui.frag',
  'shaders/gui.vert',
  'shaders/gui_uint.frag',
  'shaders/hdr_compositing.frag',
  'shaders/min_lod_map.comp',
  'shaders/prepare_draw_indirect.comp.glsl',
  'shaders/shadow_bounds.comp.glsl',
  # 'shaders/shadowmap.frag',
  # 'shaders/shadowmap.vert',
  'shaders/sky/multiscat_lut.comp',
  'shaders/sky/sky_raymarch.frag',
  'shaders/sky/skyview_lut.frag',
  'shaders/sky/transmittance_lut.frag',
  'shaders/temporal_accumulation.comp.glsl',
  'shaders/tonemap.comp.glsl',
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
  command: ['py', '-c', 'import os; os.mkdir("' + meson.project_build_root() / 'shaders'+ '")'],
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
shaders = declare_dependency(sources: shader_targets)

# Main target
executable('test-vulkan', main_src,
  include_directories: [main_inc, thirdparty_inc, fmt_inc],
  dependencies: [vulkan, shaders],
  link_with: [imgui, spirv_reflect, simdjson, fmt],
  override_options : ['cpp_std=c++latest'],
  cpp_args: [
    '-DNOMINMAX',
    '-DVC_EXTRALEAN',
    '-DWIN32_LEAN_AND_MEAN',
    '-DUNICODE',
    '-DIMGUI_USER_CONFIG="imgui_config.h"',
    '-DVMA_DEBUG_INITIALIZE_ALLOCATIONS=1',
    '-DVMA_DEBUG_MARGIN=16',
    '-DVMA_DEBUG_DETECT_CORRUPTION=1'
  ],
  link_args: ['-limm32']
)
