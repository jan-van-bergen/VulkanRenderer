#pragma once
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

#include "VulkanMemory.h"

#include "Types.h"

#include "Vector2.h"
#include "Transform.h"

#include "Texture.h"
#include "Material.h"

typedef int MeshHandle;

struct Mesh {
	struct Vertex {
		Vector3 position;
		Vector2 texcoord;
		Vector3 normal;

		static std::vector<VkVertexInputBindingDescription> get_binding_descriptions() {
			std::vector<VkVertexInputBindingDescription> binding_descriptions(1);

			binding_descriptions[0].binding = 0;
			binding_descriptions[0].stride = sizeof(Vertex);
			binding_descriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

			return binding_descriptions;
		}

		static std::vector<VkVertexInputAttributeDescription> get_attribute_descriptions() {
			std::vector<VkVertexInputAttributeDescription> attribute_descriptions(3);

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

			return attribute_descriptions;
		}
	};
	
	VulkanMemory::Buffer vertex_buffer;
	VulkanMemory::Buffer index_buffer;

	struct SubMesh {
		int index_offset;
		int index_count;

		struct AABB {
			Vector3 min;
			Vector3 max;
		} aabb;

		TextureHandle texture_handle;
	};

	std::vector<SubMesh> sub_meshes;

	static inline std::vector<Mesh> meshes;

	static void free();
};

struct MeshInstance {
	std::string name;

	MeshHandle mesh_handle;

	Mesh & get_mesh() const { return Mesh::meshes[mesh_handle]; }
	
	Material * material;

	Transform transform;
};
