#version 450
#include "util.h"

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec2 in_texcoord;
layout(location = 2) in vec3 in_normal;

layout(location = 0) out vec4 out_albedo;
layout(location = 1) out vec4 out_normal_roughness_metallic;

layout(binding = 1) uniform sampler2D sampler_diffuse;

layout(set = 1, binding = 0) uniform MaterialUBO {
	float roughness;
	float metallic;
} material;

void main() {
	vec4 diffuse =  texture(sampler_diffuse, in_texcoord);

	if (diffuse.a < 0.95f) discard;

	out_albedo = diffuse;
	out_normal_roughness_metallic = vec4(
		pack_normal(in_normal),
		material.roughness,
		material.metallic
	);
}
