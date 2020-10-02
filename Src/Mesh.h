#pragma once
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

#include "VulkanMemory.h"

#include "Types.h"

#include "Vector2.h"
#include "Vector3.h"

struct Mesh {
	struct Vertex {
		Vector3 position;
		Vector2 texcoord;
		Vector3 normal;

		static VkVertexInputBindingDescription get_binding_description() {
			VkVertexInputBindingDescription binding_description = { };
			binding_description.binding = 0;
			binding_description.stride = sizeof(Vertex);
			binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

			return binding_description;
		}

		static std::vector<VkVertexInputAttributeDescription> get_attribute_description() {
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

	std::vector<Vertex> vertices;
	std::vector<u32>    indices;

	VulkanMemory::Buffer vertex_buffer;
	VulkanMemory::Buffer index_buffer;

	static Mesh * load(std::string const & filename);

	static void free();
};
