#version 450

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec2 in_texcoord;
layout(location = 2) in vec3 in_normal;

layout(push_constant, row_major) uniform PushConstants {
	mat4 wvp;
};

void main() {
	gl_Position = wvp * vec4(in_position, 1.0f);
}
