#version 450
#include "lighting.h"
#include "util.h"

layout(location = 0) in vec2 in_uv;

layout(location = 0) out vec4 out_colour;

layout(binding = 0) uniform sampler2D sampler_albedo;
layout(binding = 1) uniform sampler2D sampler_normal;
layout(binding = 2) uniform sampler2D sampler_depth;

layout(binding = 3, row_major) uniform UniformBuffer {
	DirectionalLight directional_light;
	
	vec3 camera_position;

	mat4 inv_view_projection;
};

layout(set = 1, binding = 0) uniform sampler2D sampler_shadow_map;

void main() {
	vec3  albedo        = texture(sampler_albedo, in_uv).rgb;
	vec2  packed_normal = texture(sampler_normal, in_uv).rg;
	float depth         = texture(sampler_depth,  in_uv).r;

	// Don't light the Sky
	if (packed_normal == vec2(0.0f)) {
		out_colour = vec4(albedo, 1.0f);
		return;
	}

	// Reconstruct Clip Space position
	vec4 position;
	position.xy = 2.0f * in_uv.xy - 1.0f;
	position.z  = depth;
	position.w  = 1.0f;

	// Transform position from Clip Space to World Space
	position  = inv_view_projection * position;
	position /= position.w;

	vec3 normal = unpack_normal(packed_normal);

	const float ambient = 0.1f;

	out_colour = vec4(calc_directional_light(directional_light, albedo, position.xyz, normal, camera_position, sampler_shadow_map), 1.0f);
}
