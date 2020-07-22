#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 in_texcoord;
layout(location = 1) in vec3 in_colour;

layout(location = 0) out vec4 out_colour;

layout(binding = 1) uniform sampler2D texture_sampler;

void main() {
    out_colour = texture(texture_sampler, in_texcoord); // vec4(in_colour, 1.0f);
}
