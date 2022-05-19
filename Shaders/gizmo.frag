#version 450

layout(location = 0) out vec4 out_colour;

layout(push_constant, row_major) uniform PushConstants {
	mat4 wvp;
	vec3 colour;
};

void main() {
    out_colour = vec4(colour, 1.0f);
}
