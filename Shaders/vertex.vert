#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 in_position;
layout(location = 1) in vec3 in_colour;

layout(location = 0) out vec3 out_colour;

layout(binding = 0, row_major) uniform UniformBufferObject {
	mat4 wvp;
} ubo;

void main() {
	gl_Position = ubo.wvp * vec4(in_position, 0.0f, 1.0f);
	out_colour  = in_colour;
}
