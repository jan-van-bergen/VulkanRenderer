#version 450

layout(location = 0) in vec2 in_uv;

layout(location = 0) out vec4 out_colour;

layout(binding = 0) uniform sampler2D sampler_albedo;
layout(binding = 1) uniform sampler2D sampler_position;
layout(binding = 2) uniform sampler2D sampler_normal;

void main() {
	const float ambient = 0.1f;

	vec3 albedo   = texture(sampler_albedo,   in_uv).xyz;
	vec3 position = texture(sampler_position, in_uv).xyz;
	vec3 normal   = texture(sampler_normal,   in_uv).xyz;

	vec3  light_direction = vec3(1.0f, 0.0f, 0.0f);
	float light = clamp(dot(light_direction, normal), 0.0f, 1.0f);

	light = ambient + (1.0f - ambient) * light;
	
	out_colour = vec4(light * albedo, 1.0f);
}
