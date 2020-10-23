#include "VulkanMemory.h"

#include <vector>

#include "AABB.h"
#include "Camera.h"

struct Gizmo {
private:
	AABB aabb;

	void calc_aabb();

public:
	struct Vertex {
		Vector3 position;

		static std::vector<VkVertexInputBindingDescription> get_binding_descriptions() {
			std::vector<VkVertexInputBindingDescription> binding_descriptions(1);

			binding_descriptions[0].binding = 0;
			binding_descriptions[0].stride = sizeof(Vertex);
			binding_descriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

			return binding_descriptions;
		}

		static std::vector<VkVertexInputAttributeDescription> get_attribute_descriptions() {
			std::vector<VkVertexInputAttributeDescription> attribute_descriptions(1);

			// Position
			attribute_descriptions[0].binding  = 0;
			attribute_descriptions[0].location = 0;
			attribute_descriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
			attribute_descriptions[0].offset = offsetof(Vertex, position);

			return attribute_descriptions;
		}
	};

	VulkanMemory::Buffer vertex_buffer;
	VulkanMemory::Buffer index_buffer;

	std::vector<Vertex> vertices;
	std::vector<int>    indices;

	bool intersects_mouse(Camera const & camera, int mouse_x, int mouse_y);

	static Gizmo generate_position();
	static Gizmo generate_rotation();
	//static Gizmo generate_scale();
};
