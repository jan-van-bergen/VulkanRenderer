#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 in_texcoord;
layout(location = 1) in vec3 in_normal;

layout(location = 0) out vec4 out_colour;

layout(binding = 1) uniform sampler2D texture_samplers[2];

layout(binding = 2) uniform UBO {
	int texture_index;
};

void main() {
	const float ambient = 0.1f;

	vec3  light_direction = vec3(1.0f, 0.0f, 0.0f);
	float light = clamp(dot(light_direction, in_normal), 0.0f, 1.0f);

	light = ambient + (1.0f - ambient) * light;

	out_colour = vec4(light.xxx, 1.0f) * texture(texture_samplers[texture_index], in_texcoord);
}
