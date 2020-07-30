layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV0;
layout (location = 3) in vec2 inUV1;
layout (location = 4) in vec4 inJoint0;
layout (location = 5) in vec4 inWeight0;

layout (location = 0) out vec3 outPosition;
layout (location = 1) out vec3 outNormal;
layout (location = 2) out vec2 outUV0;
layout (location = 3) out vec2 outUV1;
layout (location = 4) out vec4 outJoint0;
layout (location = 5) out vec4 outWeight0;
layout (location = 6) out vec4 outLightPosition;

#include "globals.h"

layout (set = 2, binding = 0) uniform UBONode {
    mat4 transform;
} node;

void main()
{
    vec4 locPos = node.transform * vec4(inPosition, 1.0);
    outNormal = normalize(transpose(inverse(mat3(node.transform))) * inNormal);
    outPosition = locPos.xyz / locPos.w;
    outUV0 = inUV0;
    outUV1 = inUV1;
    outJoint0 = inJoint0;
    outWeight0 = inWeight0;
    outLightPosition = global.sun_proj * global.sun_view * vec4(outPosition, 1.0);
    gl_Position = global.camera_proj * global.camera_view * vec4(outPosition, 1.0);
    // debug shadow map :)
    // gl_Position = outLightPosition;
}
