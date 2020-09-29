#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec2 in_texcoord;
layout(location = 2) in vec3 in_normal;

layout(location = 0) out vec2 out_texcoord;
layout(location = 1) out vec3 out_normal;

layout(push_constant, row_major) uniform PushConstants {
	mat4 world;
	mat4 wvp;
} push_constants;

void main() {
	gl_Position = push_constants.wvp * vec4(in_position, 1.0f);

	out_texcoord = in_texcoord;
	out_normal   = normalize((push_constants.world * vec4(in_normal, 0.0f)).xyz);
}
