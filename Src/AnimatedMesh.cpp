#include "AnimatedMesh.h"

#include "Scene.h"

AnimatedMeshInstance::AnimatedMeshInstance(Scene & scene, std::string const & name, AnimatedMeshHandle mesh_handle, Material * material) : scene(scene), name(name), mesh_handle(mesh_handle), material(material) {
	auto const & mesh = scene.asset_manager.get_animated_mesh(mesh_handle);

	bone_transforms.resize(mesh.bones.size());
	std::fill(bone_transforms.begin(), bone_transforms.end(), Matrix4::identity());
}

void AnimatedMeshInstance::play_animation(int index, bool restart) {
	auto & mesh = scene.asset_manager.get_animated_mesh(mesh_handle);

	if (index >= 0 && index < mesh.animations.size()) {
		current_animation = &mesh.animations[index];
		
		if (restart) current_time = 0.0f;
	} else {
		printf("WARNING: Trying to play non-existent animation with index %i!\n", index);
	}
}

void AnimatedMeshInstance::play_animation(std::string const & name, bool restart) {
	auto & mesh = scene.asset_manager.get_animated_mesh(mesh_handle);

	if (mesh.animation_names.find(name) != mesh.animation_names.end()) {
		play_animation(mesh.animation_names[name], restart);
	} else {
		printf("WARNING: Trying to play non-existent animation '%s'!\n", name.c_str());
	}
}

void AnimatedMeshInstance::stop_animation() {
	current_animation = nullptr;
}

void AnimatedMeshInstance::update(float delta) {
	if (current_animation == nullptr) return;

	auto const & mesh = scene.asset_manager.get_animated_mesh(mesh_handle);

	current_time += animation_speed * delta;

	for (int i = 0; i < mesh.bones.size(); i++) {
		auto const & bone = mesh.bones[i];

		Vector3    position = current_animation->position_channels[i].get_position(current_time, loop);
		Quaternion rotation = current_animation->rotation_channels[i].get_rotation(current_time, loop);
		
		auto local = Matrix4::create_translation(position) * Matrix4::create_rotation(rotation);

		if (bone.parent == -1) {
			assert(i == 0); // Root Bone

			bone_transforms[i] = local;
		} else {
			assert(bone.parent < i); // Parent should have been transformed BEFORE the current Bone

			bone_transforms[i] = bone_transforms[bone.parent] * local;
		}
	}

	for (int i = 0; i < mesh.bones.size(); i++) {
		bone_transforms[i] = bone_transforms[i] * mesh.bones[i].inv_bind_pose;
	}
}
