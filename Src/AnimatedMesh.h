#pragma once
#include "Animation.h"

#include "VulkanMemory.h"

#include "Types.h"
#include "Vector2.h"

#include "Transform.h"

#include "Texture.h"
#include "Material.h"

typedef int AnimatedMeshHandle;

struct AnimatedMesh {
	struct Vertex {
		inline static constexpr int MAX_BONES_PER_VERTEX = 4;

		Vector3 position;
		Vector2 texcoord;
		Vector3 normal;

		int   bone_indices[MAX_BONES_PER_VERTEX] = { };
		float bone_weights[MAX_BONES_PER_VERTEX] = { };

		static std::vector<VkVertexInputBindingDescription> get_binding_descriptions() {
			std::vector<VkVertexInputBindingDescription> binding_descriptions(1);

			binding_descriptions[0].binding = 0;
			binding_descriptions[0].stride = sizeof(Vertex);
			binding_descriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

			return binding_descriptions;
		}

		static std::vector<VkVertexInputAttributeDescription> get_attribute_descriptions() {
			std::vector<VkVertexInputAttributeDescription> attribute_descriptions(5);

			// Position
			attribute_descriptions[0].binding  = 0;
			attribute_descriptions[0].location = 0;
			attribute_descriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
			attribute_descriptions[0].offset = offsetof(Vertex, position);
		
			// Texture Coordinates
			attribute_descriptions[1].binding  = 0;
			attribute_descriptions[1].location = 1;
			attribute_descriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
			attribute_descriptions[1].offset = offsetof(Vertex, texcoord);

			// Colour
			attribute_descriptions[2].binding  = 0;
			attribute_descriptions[2].location = 2;
			attribute_descriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
			attribute_descriptions[2].offset = offsetof(Vertex, normal);

			// Bone Indices
			attribute_descriptions[3].binding  = 0;
			attribute_descriptions[3].location = 3;
			attribute_descriptions[3].format = VK_FORMAT_R32G32B32A32_SINT;
			attribute_descriptions[3].offset = offsetof(Vertex, bone_indices);
		
			// Bone Weights
			attribute_descriptions[4].binding  = 0;
			attribute_descriptions[4].location = 4;
			attribute_descriptions[4].format = VK_FORMAT_R32G32B32A32_SFLOAT;
			attribute_descriptions[4].offset = offsetof(Vertex, bone_weights);

			return attribute_descriptions;
		}
	};
	
	VulkanMemory::Buffer vertex_buffer;
	VulkanMemory::Buffer index_buffer;
	
	struct Bone {
		std::string name;

		int parent;
		
		Matrix4 inv_bind_pose;
	};
	
	std::vector<Bone> bones;
	
	struct SubMesh {
		int index_offset;
		int index_count;

		TextureHandle texture_handle;
	};

	std::vector<SubMesh> sub_meshes;
	
	std::unordered_map<std::string, int> animation_names;
	std::vector<Animation>               animations;
	
	static inline std::vector<VulkanMemory::Buffer> storage_buffer_bones;

	static inline std::vector<AnimatedMesh> meshes;

	static void free();
};

struct AnimatedMeshInstance {
	std::string name;

	AnimatedMeshHandle mesh_handle;
	
	Material * material;

	Transform transform;
	
	float       current_time = 0.0f;
	Animation * current_animation = nullptr;

	bool  loop = true;
	float animation_speed = 1.0f;
	
	std::vector<Matrix4> bone_transforms;

	AnimatedMeshInstance(std::string const & name, AnimatedMeshHandle mesh_handle, Material * material);

	void play_animation(int index,                bool restart = false);
	void play_animation(std::string const & name, bool restart = false);
	void stop_animation();
	
	void update(float time);
	
	bool is_playing() const { return current_animation != nullptr; }
	
	AnimatedMesh & get_mesh() const { return AnimatedMesh::meshes[mesh_handle]; }
};
