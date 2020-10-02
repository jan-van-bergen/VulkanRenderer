#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec2 in_texcoord;
layout(location = 2) in vec3 in_normal;

layout(location = 0) out vec4 out_albedo;
layout(location = 1) out vec4 out_position;
layout(location = 2) out vec4 out_normal;

layout(binding = 1) uniform sampler2D texture_samplers[2];

layout(binding = 2) uniform UBO {
	int texture_index;
};

void main() {
	out_albedo   = texture(texture_samplers[texture_index], in_texcoord);
	out_position = vec4(in_position, 0.0f);
	out_normal   = vec4(in_normal,   0.0f);
}
