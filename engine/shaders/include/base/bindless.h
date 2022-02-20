// -*- mode: glsl; -*-

#ifndef BINDLESS_H
#define BINDLESS_H

#include "base/types.h"

// #extension GL_EXT_buffer_reference : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_ARB_shader_draw_parameters : require

#define GLOBAL_SAMPLER_BINDING 0
#define GLOBAL_IMAGE_BINDING 1
#define GLOBAL_BUFFER_BINDING 2

#define GLOBAL_BINDLESS_SET 0
#define GLOBAL_UNIFORM_SET 1
#define SHADER_UNIFORM_SET 2

layout(set = GLOBAL_BINDLESS_SET, binding = GLOBAL_SAMPLER_BINDING) uniform sampler2D global_textures[];
layout(set = GLOBAL_BINDLESS_SET, binding = GLOBAL_SAMPLER_BINDING) uniform usampler2D global_textures_uint[];
layout(set = GLOBAL_BINDLESS_SET, binding = GLOBAL_SAMPLER_BINDING) uniform sampler3D global_textures_3d[];
layout(set = GLOBAL_BINDLESS_SET, binding = GLOBAL_SAMPLER_BINDING) uniform usampler3D global_textures_3d_uint[];

layout(set = GLOBAL_BINDLESS_SET, binding = GLOBAL_IMAGE_BINDING, rgba8) uniform image2D global_images_2d_rgba8[];
layout(set = GLOBAL_BINDLESS_SET, binding = GLOBAL_IMAGE_BINDING, rgba16f) uniform image2D global_images_2d_rgba16f[];
layout(set = GLOBAL_BINDLESS_SET, binding = GLOBAL_IMAGE_BINDING, rgba32f) uniform image2D global_images_2d_rgba32f[];
layout(set = GLOBAL_BINDLESS_SET, binding = GLOBAL_IMAGE_BINDING, r32f) uniform image2D global_images_2d_r32f[];


#endif
