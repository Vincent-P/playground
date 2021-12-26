// -*- mode: glsl; -*-

#ifndef BINDLESS_H
#define BINDLESS_H

#include "base/types.h"

// #extension GL_EXT_buffer_reference : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_ARB_shader_draw_parameters : require

#define GLOBAL_UNIFORM_SET 0
#define GLOBAL_SAMPLER_SET 1
#define GLOBAL_IMAGE_SET 2
#define GLOBAL_BUFFER_SET 3
#define SHADER_SET 4

layout(set = GLOBAL_SAMPLER_SET, binding = 0) uniform sampler2D global_textures[];
layout(set = GLOBAL_SAMPLER_SET, binding = 0) uniform usampler2D global_textures_uint[];
layout(set = GLOBAL_SAMPLER_SET, binding = 0) uniform sampler3D global_textures_3d[];
layout(set = GLOBAL_SAMPLER_SET, binding = 0) uniform usampler3D global_textures_3d_uint[];

layout(set = GLOBAL_IMAGE_SET, binding = 0, rgba8) uniform image2D global_images_2d_rgba8[];
layout(set = GLOBAL_IMAGE_SET, binding = 0, rgba16f) uniform image2D global_images_2d_rgba16f[];
layout(set = GLOBAL_IMAGE_SET, binding = 0, rgba32f) uniform image2D global_images_2d_rgba32f[];
layout(set = GLOBAL_IMAGE_SET, binding = 0, r32f) uniform image2D global_images_2d_r32f[];


#endif
