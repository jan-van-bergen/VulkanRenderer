#include "Mesh.h"

#include <algorithm>
#include <unordered_map>

#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/Importer.hpp> 
#include <assimp/material.h>

#include "VulkanContext.h"
#include "VulkanMemory.h"

#include "Util.h"

static std::unordered_map<std::string, MeshHandle> cache;

MeshHandle Mesh::load(std::string const & filename) {
	auto & mesh_handle = cache[filename];

	if (mesh_handle != 0 && meshes.size() > 0) return mesh_handle;
	
	std::string path = filename.substr(0, filename.find_last_of("/\\") + 1);

	mesh_handle = meshes.size();
	auto & mesh = meshes.emplace_back();

	Assimp::Importer Importer;
	aiScene const * assimp_scene = Importer.ReadFile(filename, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices);

	for (int i = 0; i < assimp_scene->mNumMeshes; i++) {
		auto assimp_mesh = assimp_scene->mMeshes[i];
		
		std::vector<Vertex> vertices(assimp_mesh->mNumVertices);
		std::vector<u32>    indices (assimp_mesh->mNumFaces * 3);
		
		auto & sub_mesh = mesh.sub_meshes.emplace_back();
		sub_mesh.index_count = indices.size();

		sub_mesh.aabb.min = Vector3(+INFINITY);
		sub_mesh.aabb.max = Vector3(-INFINITY);

		// Copy Vertices
		for (int i = 0; i < assimp_mesh->mNumVertices; i++) {
			vertices[i].position = Vector3(
				assimp_mesh->mVertices[i].x,
				assimp_mesh->mVertices[i].y,
				assimp_mesh->mVertices[i].z
			);
			vertices[i].texcoord = Vector2(
				assimp_mesh->mTextureCoords[0][i].x,
				assimp_mesh->mTextureCoords[0][i].y
			);
			vertices[i].normal = Vector3(
				assimp_mesh->mNormals[i].x,
				assimp_mesh->mNormals[i].y,
				assimp_mesh->mNormals[i].z
			);

			sub_mesh.aabb.min = Vector3::min(sub_mesh.aabb.min, vertices[i].position);
			sub_mesh.aabb.max = Vector3::max(sub_mesh.aabb.max, vertices[i].position);
		}

		// Copy Indices
		for (int i = 0; i < assimp_mesh->mNumFaces; i++) {
			indices[i * 3    ] = assimp_mesh->mFaces[i].mIndices[0];
			indices[i * 3 + 1] = assimp_mesh->mFaces[i].mIndices[1];
			indices[i * 3 + 2] = assimp_mesh->mFaces[i].mIndices[2];
		}

		// Upload Vertex and Index Buffer
		auto buffer_size_vertices = Util::vector_size_in_bytes(vertices);
		auto buffer_size_indices  = Util::vector_size_in_bytes(indices);

		sub_mesh.vertex_buffer = VulkanMemory::buffer_create(buffer_size_vertices, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		sub_mesh.index_buffer  = VulkanMemory::buffer_create(buffer_size_indices,  VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		VulkanMemory::buffer_copy_staged(sub_mesh.vertex_buffer, vertices.data(), buffer_size_vertices);
		VulkanMemory::buffer_copy_staged(sub_mesh.index_buffer,  indices .data(), buffer_size_indices);

		sub_mesh.texture_handle = -1;

		int material_id = assimp_mesh->mMaterialIndex;
		if (material_id != -1) {
			auto assimp_material = assimp_scene->mMaterials[material_id];

			aiString texture_path; 
			bool has_texture = assimp_material->GetTexture(aiTextureType_DIFFUSE, 0, &texture_path) == AI_SUCCESS;

			if (has_texture) sub_mesh.texture_handle = Texture::load(path + std::string(texture_path.data));
		}

		if (sub_mesh.texture_handle == -1) sub_mesh.texture_handle = Texture::load("Data/bricks.png");
	}

	// Sort Submeshes so that Submeshes with the same Texture are contiguous
	std::sort(mesh.sub_meshes.begin(), mesh.sub_meshes.end(), [](auto const & a, auto const & b) {
		return a.texture_handle < b.texture_handle;
	});

	return mesh_handle;
}

void Mesh::free() {
	for (auto & mesh : meshes) {
		for (auto & sub_mesh : mesh.sub_meshes) {
			VulkanMemory::buffer_free(sub_mesh.vertex_buffer);
			VulkanMemory::buffer_free(sub_mesh.index_buffer);
		}
	}

	meshes.clear();
	cache.clear();
}
