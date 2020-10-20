#include "AnimatedMesh.h"

static Matrix4 const y_to_z = { // -90 deg rotation around x-axis
	1.0f,  0.0f, 0.0f, 0.0f,
	0.0f,  0.0f, 1.0f, 0.0f,
	0.0f, -1.0f, 0.0f, 0.0f,
	0.0f,  0.0f, 0.0f, 1.0f
};

void AnimatedMesh::free() {
	for (auto & mesh : meshes) {
		VulkanMemory::buffer_free(mesh.vertex_buffer);
		VulkanMemory::buffer_free(mesh.index_buffer);
	}

	meshes.clear();
}

void AnimatedMesh::play_animation(std::string const & name) {
	if (animations.find(name) != animations.end()) {
		current_animation = &animations[name];
	} else {
		printf("WARNING: Trying to play non-existent animation %s!\n", name.c_str());
	}
}

void AnimatedMesh::stop_animation() {
	current_animation = nullptr;
}

void AnimatedMesh::update(float time) {
	if (current_animation == nullptr) return;

	for (auto & bone : bones) {
		if (current_animation->channels.find(bone.name) == current_animation->channels.end()) __debugbreak();

		auto & channel = current_animation->channels[bone.name];

		Vector3    position;
		Quaternion rotation;
		channel.get_pose(15.0f * time, &position, &rotation);

		auto local = Matrix4::create_translation(position) * Matrix4::create_rotation(rotation);

		if (bone.parent == -1) {
			bone.current_pose = y_to_z * local;
		} else {
			bone.current_pose = bones[bone.parent].current_pose * local;
		}
	}

	for (auto & bone : bones) {
		bone.current_pose = bone.current_pose * bone.inv_bind_pose;
	}
}
