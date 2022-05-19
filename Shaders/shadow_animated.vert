#version 450

layout(location = 0) in  vec3 in_position;
layout(location = 1) in  vec2 in_texcoord;
layout(location = 2) in  vec3 in_normal;
layout(location = 3) in ivec4 in_bone_indices;
layout(location = 4) in  vec4 in_bone_weights;

layout(push_constant, row_major) uniform PushConstants {
	mat4 wvp;
	int bone_offset;
};

layout(set = 1, binding = 0, row_major) buffer readonly Bones {
	mat4 bones[];
};

void main() {
	mat4 skinned =
		bones[bone_offset + in_bone_indices.x] * in_bone_weights.x +
		bones[bone_offset + in_bone_indices.y] * in_bone_weights.y +
		bones[bone_offset + in_bone_indices.z] * in_bone_weights.z +
		bones[bone_offset + in_bone_indices.w] * in_bone_weights.w;

	gl_Position = wvp * skinned * vec4(in_position, 1.0f);
}
