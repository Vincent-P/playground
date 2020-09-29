layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec2 inUV;
layout (location = 0) out vec3 outPosition;
layout (location = 1) out vec2 outUV;
layout (location = 2) out vec3 outCameraPosition;

#include "globals.h"

void main()
{
    float size = 100.0;
    float4 position = float4(inPosition.x*size, inPosition.y, inPosition.z*size, 1.0);
    outUV = inUV * size;
    gl_Position = global.camera_proj * global.camera_view * position;
    outCameraPosition = (global.camera_view * position).xyz;
}
