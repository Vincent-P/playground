#include "types.h"
#include "globals.h"

layout(location = 0) out float4 o_color;
void main()
{
    o_color = float4(float3(gl_FragCoord.z), 1);
}
