#version 450

layout(location = 0) in vec3 in_position;

layout(location = 0) out noperspective vec2 out_uv;

layout(push_constant, row_major) uniform PushConstants {
	mat4 wvp;
};

void main() {
	gl_Position = wvp * vec4(in_position, 1.0f);

	// Perspective divide and convert from [-1, 1] to [0, 1]
    out_uv = 0.5f + 0.5f * gl_Position.xy / gl_Position.w;
}
