layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec2 inUV;
layout (location = 0) out vec3 outPosition;
layout (location = 1) out vec2 outUV;
layout (location = 2) out vec3 outCameraPosition;

#include "globals.h"

void main()
{
    float size = 100.0;
    outUV = inUV * size;
    gl_Position = global.camera_proj * global.camera_view * vec4(inPosition.xyz*size, 1.0);
    outCameraPosition = (global.camera_view * float4(inPosition.xyz*size, 1.0)).xyz;
}
