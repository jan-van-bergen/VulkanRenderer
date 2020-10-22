#include "AssetLoader.h"

#include <algorithm>
#include <filesystem>

#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/Importer.hpp> 
#include <assimp/material.h>
#include <assimp/pbrmaterial.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image/stb_image.h>

#include "VulkanCheck.h"
#include "VulkanContext.h"

MeshHandle AssetLoader::load_mesh(std::string const & filename) {
	auto & mesh_handle = cached_meshes[filename];

	if (mesh_handle != 0) return mesh_handle - 1;
	
	std::string path = filename.substr(0, filename.find_last_of("/\\") + 1);

	mesh_handle = Mesh::meshes.size() + 1;
	auto & mesh = Mesh::meshes.emplace_back();
	
	Assimp::Importer assimp_importer;
	aiScene const * assimp_scene = assimp_importer.ReadFile(filename, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices);
	
	if (assimp_scene == nullptr) {
		printf("ERROR: Unable to load Mesh %s!\n", filename.c_str());
		abort();
	}

	// Sum up total amount of Vertices/Indices in all Sub Meshes
	int total_num_vertices = 0;
	int total_num_indices  = 0;

	for (int i = 0; i < assimp_scene->mNumMeshes; i++) {
		auto assimp_mesh = assimp_scene->mMeshes[i];

		total_num_vertices += assimp_mesh->mNumVertices;
		total_num_indices  += assimp_mesh->mNumFaces * 3;
	}

	std::vector<Mesh::Vertex> vertices(total_num_vertices);
	std::vector<u32>          indices (total_num_indices);
		
	auto offset_vertex = 0;
	auto offset_index  = 0;

	for (int m = 0; m < assimp_scene->mNumMeshes; m++) {
		auto assimp_mesh = assimp_scene->mMeshes[m];
		
		int num_vertices = assimp_mesh->mNumVertices;
		int num_indices  = assimp_mesh->mNumFaces * 3;

		auto & sub_mesh = mesh.sub_meshes.emplace_back();
		sub_mesh.index_offset = offset_index;
		sub_mesh.index_count  = num_indices;

		sub_mesh.aabb.min = Vector3(+INFINITY);
		sub_mesh.aabb.max = Vector3(-INFINITY);

		// Load Vertices
		for (int i = 0; i < assimp_mesh->mNumVertices; i++) {
			auto index = offset_vertex + i;

			vertices[index].position = Vector3(
				assimp_mesh->mVertices[i].x,
				assimp_mesh->mVertices[i].y,
				assimp_mesh->mVertices[i].z
			);
			vertices[index].texcoord = Vector2(
				assimp_mesh->mTextureCoords[0][i].x,
				assimp_mesh->mTextureCoords[0][i].y
			);
			vertices[index].normal = Vector3(
				assimp_mesh->mNormals[i].x,
				assimp_mesh->mNormals[i].y,
				assimp_mesh->mNormals[i].z
			);

			sub_mesh.aabb.min = Vector3::min(sub_mesh.aabb.min, vertices[index].position);
			sub_mesh.aabb.max = Vector3::max(sub_mesh.aabb.max, vertices[index].position);
		}

		// Load Indices
		for (int i = 0; i < assimp_mesh->mNumFaces; i++) {
			auto index = offset_index + i * 3;

			indices[index    ] = offset_vertex + assimp_mesh->mFaces[i].mIndices[0];
			indices[index + 1] = offset_vertex + assimp_mesh->mFaces[i].mIndices[1];
			indices[index + 2] = offset_vertex + assimp_mesh->mFaces[i].mIndices[2];
		}

		// Load Material
		sub_mesh.texture_handle = -1;

		int material_id = assimp_mesh->mMaterialIndex;
		if (material_id != -1) {
			auto assimp_material = assimp_scene->mMaterials[material_id];

			aiString texture_path; 
			auto has_texture = assimp_material->GetTexture(aiTextureType_DIFFUSE, 0, &texture_path) == AI_SUCCESS;

			if (has_texture) sub_mesh.texture_handle = load_texture(path + std::string(texture_path.C_Str()));
		}

		if (sub_mesh.texture_handle == -1) sub_mesh.texture_handle = load_texture("Data/bricks.png");

		offset_vertex += num_vertices;
		offset_index  += num_indices;
	}

	assert(offset_vertex == total_num_vertices);
	assert(offset_index  == total_num_indices);

	// Upload Vertex and Index Buffer
	auto buffer_size_vertices = Util::vector_size_in_bytes(vertices);
	auto buffer_size_indices  = Util::vector_size_in_bytes(indices);

	mesh.vertex_buffer = VulkanMemory::buffer_create(buffer_size_vertices, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	mesh.index_buffer  = VulkanMemory::buffer_create(buffer_size_indices,  VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	VulkanMemory::buffer_copy_staged(mesh.vertex_buffer, vertices.data(), buffer_size_vertices);
	VulkanMemory::buffer_copy_staged(mesh.index_buffer,  indices .data(), buffer_size_indices);

	// Sort Submeshes so that Submeshes with the same Texture are contiguous
	std::sort(mesh.sub_meshes.begin(), mesh.sub_meshes.end(), [](auto const & a, auto const & b) {
		return a.texture_handle < b.texture_handle;
	});

	return mesh_handle - 1;
}

void init_bone_hierarchy(AnimatedMesh & mesh, aiNode const * assimp_node, int parent_index, std::vector<int> & displacement, int * new_index) {
	auto bone_index = -1;

	for (int b = 0; b < mesh.bones.size(); b++) {
		if (strcmp(mesh.bones[b].name.c_str(), assimp_node->mName.C_Str()) == 0) {
			bone_index = b;

			break;
		}
	}

	if (bone_index == -1) {
		for (int c = 0; c < assimp_node->mNumChildren; c++) {
			init_bone_hierarchy(mesh, assimp_node->mChildren[c], parent_index, displacement, new_index);
		}

		return;
	}

	auto & bone = mesh.bones[bone_index];
	bone.parent = parent_index;

	assert(*new_index < displacement.size());
	displacement[bone_index] = (*new_index)++;
	
	for (int c = 0; c < assimp_node->mNumChildren; c++) {
		init_bone_hierarchy(mesh, assimp_node->mChildren[c], displacement[bone_index], displacement, new_index);
	}
}

AnimatedMeshHandle AssetLoader::load_animated_mesh(std::string const & filename) {
	auto & mesh_handle = cached_animated_meshes[filename];

	if (mesh_handle != 0) return mesh_handle - 1;
	
	std::string path = filename.substr(0, filename.find_last_of("/\\") + 1);

	mesh_handle = AnimatedMesh::meshes.size() + 1;
	auto & mesh = AnimatedMesh::meshes.emplace_back();

	Assimp::Importer assimp_importer;
	aiScene const * assimp_scene = assimp_importer.ReadFile(filename, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices);
	
	if (assimp_scene == nullptr) {
		printf("ERROR: Unable to load Mesh %s!\n", filename.c_str());
		abort();
	}
	
	// Sum up total amount of Vertices/Indices in all Sub Meshes
	int total_num_vertices = 0;
	int total_num_indices  = 0;

	for (int i = 0; i < assimp_scene->mNumMeshes; i++) {
		auto assimp_mesh = assimp_scene->mMeshes[i];

		total_num_vertices += assimp_mesh->mNumVertices;
		total_num_indices  += assimp_mesh->mNumFaces * 3;
	}

	std::vector<AnimatedMesh::Vertex> vertices(total_num_vertices);
	std::vector<u32>                  indices (total_num_indices);
	
	std::unordered_map<std::string, int> bones;

	auto offset_vertex = 0;
	auto offset_index  = 0;
	
	for (int m = 0; m < assimp_scene->mNumMeshes; m++) {
		auto assimp_mesh = assimp_scene->mMeshes[m];
		
		auto num_vertices = assimp_mesh->mNumVertices;
		auto num_indices  = assimp_mesh->mNumFaces * 3;
		auto num_bones    = assimp_mesh->mNumBones;

		auto & sub_mesh = mesh.sub_meshes.emplace_back();
		sub_mesh.index_offset = offset_index;
		sub_mesh.index_count  = num_indices;

		// Load Vertices
		for (int i = 0; i < assimp_mesh->mNumVertices; i++) {
			auto index = offset_vertex + i;

			vertices[index].position = Vector3(
				assimp_mesh->mVertices[i].x,
				assimp_mesh->mVertices[i].y,
				assimp_mesh->mVertices[i].z
			);
			vertices[index].texcoord = Vector2(
				assimp_mesh->mTextureCoords[0][i].x,
				assimp_mesh->mTextureCoords[0][i].y
			);
			vertices[index].normal = Vector3(
				assimp_mesh->mNormals[i].x,
				assimp_mesh->mNormals[i].y,
				assimp_mesh->mNormals[i].z
			);
		}

		// Load Indices
		for (int i = 0; i < assimp_mesh->mNumFaces; i++) {
			auto index = offset_index + i * 3;

			indices[index    ] = offset_vertex + assimp_mesh->mFaces[i].mIndices[0];
			indices[index + 1] = offset_vertex + assimp_mesh->mFaces[i].mIndices[1];
			indices[index + 2] = offset_vertex + assimp_mesh->mFaces[i].mIndices[2];
		}
		
		// Load Bones
		for (int b = 0; b < assimp_mesh->mNumBones; b++) {
			auto assimp_bone = assimp_mesh->mBones[b];

			std::string bone_name = assimp_bone->mName.C_Str();

			int bone_index;
			
			if (bones.find(bone_name) == bones.end()) {
				bone_index  = mesh.bones.size();
				auto & bone = mesh.bones.emplace_back();
				
				bone.name = bone_name;
				std::memcpy(&bone.inv_bind_pose.cells, &assimp_bone->mOffsetMatrix, 4 * 4 * sizeof(float));

				bones.insert(std::make_pair(bone_name, bone_index));
			} else {
				bone_index = bones[bone_name];
			}

			for (int w = 0; w < assimp_bone->mNumWeights; w++) {
				auto const & assimp_weight = assimp_bone->mWeights[w];

				auto & vertex = vertices[offset_vertex + assimp_weight.mVertexId];

				auto bone_offset = 0;
				auto bone_valid = true;

				while (vertex.bone_weights[bone_offset] != 0.0f) {
					bone_offset++;

					if (bone_offset == AnimatedMesh::Vertex::MAX_BONES_PER_VERTEX) {
						bone_valid = false; // Maximum number of bones exceeded!
						
						// Re-normalize so the weight add up to 1
						auto weight_sum = 0.0f;

						for (int i = 0; i < AnimatedMesh::Vertex::MAX_BONES_PER_VERTEX; i++) weight_sum += vertex.bone_weights[i];
						for (int i = 0; i < AnimatedMesh::Vertex::MAX_BONES_PER_VERTEX; i++) vertex.bone_weights[i] /= weight_sum;

						break;
					}
				}

				if (bone_valid) {
					vertex.bone_indices[bone_offset] = bone_index;
					vertex.bone_weights[bone_offset] = assimp_weight.mWeight;
				}
			}
		}

		// Load Material
		sub_mesh.texture_handle = -1;

		int material_id = assimp_mesh->mMaterialIndex;
		if (material_id != -1) {
			auto assimp_material = assimp_scene->mMaterials[material_id];

			aiString texture_path; 
			auto has_texture = assimp_material->GetTexture(aiTextureType_DIFFUSE, 0, &texture_path) == AI_SUCCESS;

			if (has_texture) {
				auto texture_filename = std::string(texture_path.C_Str());
				
				if (std::filesystem::exists(texture_filename)) {
					sub_mesh.texture_handle = load_texture(texture_filename);
				} else {
					texture_filename = path + texture_filename;

					if (std::filesystem::exists(texture_filename)) {
						sub_mesh.texture_handle = load_texture(texture_filename);
					}
				}
			}
		}

		if (sub_mesh.texture_handle == -1) sub_mesh.texture_handle = load_texture("Data/bricks.png");
		
		offset_vertex += num_vertices;
		offset_index  += num_indices;
	}
	
	assert(offset_vertex == total_num_vertices);
	assert(offset_index  == total_num_indices);

	// Load Animations
	for (int a = 0; a < assimp_scene->mNumAnimations; a++) {
		auto assimp_animation = assimp_scene->mAnimations[a];

		mesh.animation_names[std::string(assimp_animation->mName.C_Str())] = mesh.animations.size();
		auto & animation = mesh.animations.emplace_back();

		for (int c = 0; c < assimp_animation->mNumChannels; c++) {
			auto assimp_channel = assimp_animation->mChannels[c];

			auto & position_channel = animation.position_channels.emplace_back();
			auto & rotation_channel = animation.rotation_channels.emplace_back();
			
			auto node_name = std::string(assimp_channel->mNodeName.C_Str());

			position_channel.name = node_name;
			rotation_channel.name = node_name;

			for (int k = 0; k < assimp_channel->mNumPositionKeys; k++) {
				auto const & assimp_position_key = assimp_channel->mPositionKeys[k];

				position_channel.key_frames.push_back({
					float(assimp_position_key.mTime),
					Vector3(
						assimp_position_key.mValue.x,
						assimp_position_key.mValue.y,
						assimp_position_key.mValue.z
					)
				});
			}

			for (int k = 0; k < assimp_channel->mNumRotationKeys; k++) {
				auto const & assimp_rotation_key = assimp_channel->mRotationKeys[k];

				rotation_channel.key_frames.push_back({
					float(assimp_rotation_key.mTime),
					Quaternion(
						assimp_rotation_key.mValue.x,
						assimp_rotation_key.mValue.y,
						assimp_rotation_key.mValue.z,
						assimp_rotation_key.mValue.w
					)
				});
			}
		}
	}
	
	int new_index = 0;
	std::vector<int> displacement(mesh.bones.size());
	init_bone_hierarchy(mesh, assimp_scene->mRootNode, -1, displacement, &new_index);
	
	for (int v = 0; v < vertices.size(); v++) {
		auto & vertex = vertices[v];

		for (int i = 0; i < AnimatedMesh::Vertex::MAX_BONES_PER_VERTEX; i++) {
			if (vertex.bone_weights[i] != 0.0f) {
				vertex.bone_indices[i] = displacement[vertex.bone_indices[i]];
			}
		}
	}

	std::vector<AnimatedMesh::Bone> bones_copy(mesh.bones.size());
	for (int i = 0; i < bones_copy.size(); i++) bones_copy[displacement[i]] = mesh.bones[i];

	mesh.bones = std::move(bones_copy);
	
	// Reorder Animation Channels so that they have the same order as the Bones,
	// allowing the Channels to be indexed using the same index as the Bones
	for (int a = 0; a < mesh.animations.size(); a++) {
		auto & animation = mesh.animations[a];

		if (animation.position_channels.size() != animation.rotation_channels.size()) {
			printf("ERROR: Number of Position Channels is different from the number of Rotation Channels!\n");
			abort();
		}
		if (animation.position_channels.size() < mesh.bones.size()) {
			printf("ERROR: Number of Animation Channels is smaller than the number of Bones!\n");
			abort();
		}

		std::vector<Animation::ChannelPosition> position_channels_copy(mesh.bones.size());
		std::vector<Animation::ChannelRotation> rotation_channels_copy(mesh.bones.size());

		for (int c = 0; c < animation.position_channels.size(); c++) {
			auto & channel = animation.position_channels[c];

			for (int b = 0; b < mesh.bones.size(); b++) {
				if (channel.name == mesh.bones[b].name) {
					position_channels_copy[b] = channel;

					break;
				}
			}
		}

		for (int c = 0; c < animation.rotation_channels.size(); c++) {
			auto & channel = animation.rotation_channels[c];

			for (int b = 0; b < mesh.bones.size(); b++) {
				if (channel.name == mesh.bones[b].name) {
					rotation_channels_copy[b] = channel;

					break;
				}
			}
		}

		animation.position_channels = std::move(position_channels_copy);
		animation.rotation_channels = std::move(rotation_channels_copy);
	}

	// Upload Vertex and Index Buffer
	auto buffer_size_vertices = Util::vector_size_in_bytes(vertices);
	auto buffer_size_indices  = Util::vector_size_in_bytes(indices);

	mesh.vertex_buffer = VulkanMemory::buffer_create(buffer_size_vertices, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	mesh.index_buffer  = VulkanMemory::buffer_create(buffer_size_indices,  VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	VulkanMemory::buffer_copy_staged(mesh.vertex_buffer, vertices.data(), buffer_size_vertices);
	VulkanMemory::buffer_copy_staged(mesh.index_buffer,  indices .data(), buffer_size_indices);

	return mesh_handle - 1;
}

TextureHandle AssetLoader::load_texture(std::string const & filename) {
	auto & texture_handle = cached_textures[filename];

	if (texture_handle != 0) return texture_handle - 1;

	int texture_width;
	int texture_height;
	int texture_channels;

	auto pixels = stbi_load(filename.c_str(), &texture_width, &texture_height, &texture_channels, STBI_rgb_alpha);
	if (!pixels) {
		printf("ERROR: Unable to load Texture '%s'!\n", filename.c_str());
		//abort();
		return 0;
	}

	auto texture_size = texture_width * texture_height * 4;
	
	auto staging_buffer = VulkanMemory::buffer_create(texture_size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
	);

	VulkanMemory::buffer_copy_direct(staging_buffer, pixels, texture_size);

	stbi_image_free(pixels);

	texture_handle = Texture::textures.size() + 1;
	auto & texture = Texture::textures.emplace_back();

	auto mip_levels = 1 + u32(std::log2(std::max(texture_width, texture_height)));

	VulkanMemory::create_image(
		texture_width,
		texture_height,
		mip_levels,
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		texture.image,
		texture.image_memory
	);

	VulkanMemory::transition_image_layout(texture.image, mip_levels, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	VulkanMemory::buffer_copy_to_image(staging_buffer.buffer, texture.image, texture_width, texture_height);

	texture.generate_mipmaps(texture_width, texture_height, mip_levels);

	auto device = VulkanContext::get_device();

	VulkanMemory::buffer_free(staging_buffer);

	texture.image_view = VulkanMemory::create_image_view(texture.image, mip_levels, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
	
	VkSamplerCreateInfo sampler_create_info = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	sampler_create_info.magFilter = VK_FILTER_LINEAR;
	sampler_create_info.minFilter = VK_FILTER_LINEAR;
	sampler_create_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_create_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_create_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_create_info.anisotropyEnable = VK_TRUE;
	sampler_create_info.maxAnisotropy    = 16.0f;
	sampler_create_info.unnormalizedCoordinates = VK_FALSE;
	sampler_create_info.compareEnable = VK_FALSE;
	sampler_create_info.compareOp     = VK_COMPARE_OP_ALWAYS;
	sampler_create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	sampler_create_info.mipLodBias = 0.0f;
	sampler_create_info.minLod     = 0.0f;
	sampler_create_info.maxLod     = float(mip_levels);

	VK_CHECK(vkCreateSampler(device, &sampler_create_info, nullptr, &texture.sampler));

	return texture_handle - 1;
}
