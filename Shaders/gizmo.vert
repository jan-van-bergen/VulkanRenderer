#version 450

layout(location = 0) in vec3 in_position;

layout(binding = 0, push_constant, row_major) uniform PushConstants {
	mat4 wvp;
	vec3 colour;
};

void main() {
    gl_Position = wvp * vec4(in_position, 1.0f);
}
