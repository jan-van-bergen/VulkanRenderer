#pragma once
#include <string>
#include <unordered_map>

#include "Mesh.h"
#include "AnimatedMesh.h"

#include "Texture.h"

struct AssetLoader {
private:
	std::unordered_map<std::string, MeshHandle>         cached_meshes;
	std::unordered_map<std::string, AnimatedMeshHandle> cached_animated_meshes;
	std::unordered_map<std::string, TextureHandle>      cached_textures;

public:
	MeshHandle         load_mesh         (std::string const & filename);
	AnimatedMeshHandle load_animated_mesh(std::string const & filename);

	TextureHandle load_texture(std::string const & filename);
};
