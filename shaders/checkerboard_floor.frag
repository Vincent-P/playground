layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec2 inUV;
layout (location = 0) out vec4 outColor;

// http://iquilezles.org/www/articles/checkerfiltering/checkerfiltering.htm
float checkersGradBox(in vec2 p)
{
    // filter kernel
    vec2 w = fwidth(p) + 0.001;
    // analytical integral (box filter)
    vec2 i = 2.0*(abs(fract((p - 0.5*w)*0.5) - 0.5) - abs(fract((p + 0.5*w)*0.5) - 0.5)) / w;
    // xor pattern
    return 0.5 - 0.5*i.x*i.y;
}

void main() {
    vec3 color = vec3(1.0);

    float f = checkersGradBox(inUV);
    float k = 0.3 + f*0.1;
    outColor = vec4(color*k,1.0);
}
