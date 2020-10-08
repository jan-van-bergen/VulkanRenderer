#version 450
#include "util.h"

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec2 in_texcoord;
layout(location = 2) in vec3 in_normal;

layout(location = 0) out vec4 out_albedo;
layout(location = 1) out vec4 out_position;
layout(location = 2) out vec2 out_normal;

layout(binding = 1) uniform sampler2D sampler_diffuse;

void main() {
	out_albedo   = texture(sampler_diffuse, in_texcoord);
	out_position = vec4(in_position, 0.0f);
	out_normal   = pack_normal(in_normal);
}
