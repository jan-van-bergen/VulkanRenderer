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

	for (auto & storage_buffer : storage_buffer_bones) VulkanMemory::buffer_free(storage_buffer);
}

AnimatedMeshInstance::AnimatedMeshInstance(std::string const & name, AnimatedMeshHandle mesh_handle, Material * material) : name(name), mesh_handle(mesh_handle), material(material) {
	auto const & mesh = get_mesh();

	bone_transforms.resize(mesh.bones.size());
	std::fill(bone_transforms.begin(), bone_transforms.end(), Matrix4::identity());
}

void AnimatedMeshInstance::play_animation(std::string const & name, bool restart) {
	auto & mesh = get_mesh();

	if (mesh.animations.find(name) != mesh.animations.end()) {
		current_animation = &mesh.animations[name];
	} else {
		printf("WARNING: Trying to play non-existent animation %s!\n", name.c_str());
	}

	if (restart) current_time = 0.0f;
}

void AnimatedMeshInstance::stop_animation() {
	current_animation = nullptr;
}

void AnimatedMeshInstance::update(float delta) {
	if (current_animation == nullptr) return;

	auto const & mesh = get_mesh();

	current_time += animation_speed * delta;

	assert(bone_transforms.size() == mesh.bones.size());

	for (int i = 0; i < mesh.bones.size(); i++) {
		auto const & bone = mesh.bones[i];

		if (current_animation->channels.find(bone.name) == current_animation->channels.end()) __debugbreak();

		auto & channel = current_animation->channels[bone.name];

		Vector3    position;
		Quaternion rotation;
		channel.get_pose(current_time, &position, &rotation);

		auto local = Matrix4::create_translation(position) * Matrix4::create_rotation(rotation);

		if (bone.parent == -1) {
			bone_transforms[i] = y_to_z * local;
		} else {
			bone_transforms[i] = bone_transforms[bone.parent] * local;
		}
	}

	for (int i = 0; i < mesh.bones.size(); i++) {
		bone_transforms[i] = bone_transforms[i] * mesh.bones[i].inv_bind_pose;
	}
}
