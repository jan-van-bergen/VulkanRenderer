#pragma once
#include <string>
#include <unordered_map>

#include "Mesh.h"
#include "AnimatedMesh.h"

#include "Texture.h"

struct AssetManager {
private:
	struct Scene & scene;

	std::unordered_map<std::string, MeshHandle>         cached_meshes;
	std::unordered_map<std::string, AnimatedMeshHandle> cached_animated_meshes;
	std::unordered_map<std::string, TextureHandle>      cached_textures;

public:
	std::vector<Mesh> meshes;

	std::vector<AnimatedMesh>         animated_meshes;
	std::vector<VulkanMemory::Buffer> storage_buffer_bones;

	std::vector<Texture> textures;

	AssetManager(Scene & scene) : scene(scene) { }
	~AssetManager();

	[[nodiscard]] MeshHandle         load_mesh         (std::string const & filename);
	[[nodiscard]] AnimatedMeshHandle load_animated_mesh(std::string const & filename);

	[[nodiscard]] TextureHandle load_texture(std::string const & filename);

	Mesh & get_mesh(MeshHandle handle) { return meshes[handle]; }

	AnimatedMesh & get_animated_mesh(AnimatedMeshHandle handle) { return animated_meshes[handle]; }

	Texture & get_texture(TextureHandle handle) { return textures[handle]; }
};
