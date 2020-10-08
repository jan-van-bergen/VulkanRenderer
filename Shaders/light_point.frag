#version 450
#include "lighting.h"
#include "util.h"

layout(location = 0) in noperspective vec2 in_uv;

layout(location = 0) out vec4 out_colour;

layout(binding = 0) uniform sampler2D sampler_albedo;
layout(binding = 1) uniform sampler2D sampler_position;
layout(binding = 2) uniform sampler2D sampler_normal;

layout(binding = 3) uniform UniformBuffer {
	PointLight point_light;

	vec3 camera_position;
};

void main() {
	vec3 albedo        = texture(sampler_albedo,   in_uv).xyz;
	vec3 position      = texture(sampler_position, in_uv).xyz;
	vec2 packed_normal = texture(sampler_normal,   in_uv).xy;

	vec3 normal = unpack_normal(packed_normal);

	out_colour = vec4(albedo, 1.0f) * calc_point_light(point_light, position, normal, camera_position);
}
