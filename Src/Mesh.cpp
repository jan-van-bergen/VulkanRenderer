#include "Mesh.h"

#include <algorithm>
#include <unordered_map>

#include "VulkanContext.h"
#include "VulkanMemory.h"

#include "Util.h"

void Mesh::free() {
	for (auto & mesh : meshes) {
		VulkanMemory::buffer_free(mesh.vertex_buffer);
		VulkanMemory::buffer_free(mesh.index_buffer);
	}

	meshes.clear();
}
