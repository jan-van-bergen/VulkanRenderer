#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec2 in_texcoord;
layout(location = 2) in vec3 in_normal;

layout(location = 0) out vec4 out_albedo;
layout(location = 1) out vec4 out_position;
layout(location = 2) out vec2 out_normal;

layout(binding = 1) uniform sampler2D sampler_diffuse;

// Based on: https://knarkowicz.wordpress.com/2014/04/16/octahedron-normal-vector-encoding/
vec2 oct_wrap(vec2 v) {
	return vec2(
		(1.0f - abs(v.y)) * (v.x >= 0.0f ? +1.0f : -1.0f),
		(1.0f - abs(v.x)) * (v.y >= 0.0f ? +1.0f : -1.0f)
	);
}

vec2 pack_normal(vec3 n) {
	n /= (abs(n.x) + abs(n.y) + abs(n.z));
	n.xy = n.z >= 0.0f ? n.xy : oct_wrap(n.xy);
	return n.xy * 0.5f + 0.5f;
}

void main() {
	out_albedo   = texture(sampler_diffuse, in_texcoord);
	out_position = vec4(in_position, 0.0f);
	out_normal   = pack_normal(in_normal);
}
