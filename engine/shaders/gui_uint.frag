layout(set = 1, binding = 0) uniform usampler2D u_Texture;

layout(location = 0) in vec2 v_Texcoord;
layout(location = 1) in vec4 v_Color;

layout(location = 0) out vec4 o_Color;

void main() {
    o_Color = texture( u_Texture, v_Texcoord ) * v_Color;
}
