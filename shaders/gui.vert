layout(push_constant) uniform u_PushConstant {
    vec2 Scale;
    vec2 Translation;
} u_Parameters;

layout(location = 0) in vec2 i_Position;
layout(location = 1) in vec2 i_Texcoord;
layout(location = 2) in vec4 i_Color;

out gl_PerVertex
{
    vec4 gl_Position;
};

layout(location = 0) out vec2 v_Texcoord;
layout(location = 1) out vec4 v_Color;



void main() {
    gl_Position = vec4( i_Position * u_Parameters.Scale + u_Parameters.Translation, 0.0, 1.0 );
    v_Texcoord = i_Texcoord;
    v_Color = i_Color;
}
