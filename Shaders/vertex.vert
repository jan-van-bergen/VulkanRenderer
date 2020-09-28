#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec2 in_texcoord;
layout(location = 2) in vec3 in_colour;

layout(location = 0) out vec2 out_texcoord;
layout(location = 1) out vec3 out_colour;

layout(push_constant, row_major) uniform PushConstants {
	mat4 wvp;
} push_constants;

void main() {
	gl_Position = push_constants.wvp * vec4(in_position, 1.0f);

	out_texcoord = in_texcoord;
	out_colour   = in_colour;
}
