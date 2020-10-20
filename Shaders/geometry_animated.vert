#version 450

layout(location = 0) in  vec3 in_position;
layout(location = 1) in  vec2 in_texcoord;
layout(location = 2) in  vec3 in_normal;
layout(location = 3) in ivec4 in_bone_indices;
layout(location = 4) in  vec4 in_bone_weights;

layout(location = 0) out vec3 out_position;
layout(location = 1) out vec2 out_texcoord;
layout(location = 2) out vec3 out_normal;

layout(binding = 0, push_constant, row_major) uniform PushConstants {
	mat4 world;
	mat4 view_projection;

	int bone_offset;
};

layout(set = 2, binding = 0, row_major) buffer readonly Bones {
	mat4 bones[];
};

void main() {
	mat4 skinned = world * (
		bones[bone_offset + in_bone_indices.x] * in_bone_weights.x +
		bones[bone_offset + in_bone_indices.y] * in_bone_weights.y +
		bones[bone_offset + in_bone_indices.z] * in_bone_weights.z +
		bones[bone_offset + in_bone_indices.w] * in_bone_weights.w
	);

	vec4 world_position = skinned * vec4(in_position, 1.0f);
	gl_Position = view_projection * world_position;

	out_position = world_position.xyz;
	out_texcoord = in_texcoord;
	out_normal   = normalize((skinned * vec4(in_normal, 0.0f)).xyz);
}
