#include "Mesh.h"

Mesh::Mesh(std::vector<Vertex> const & vertices, std::vector<int> const & indices, std::vector<SubMesh> && sub_meshes) :
	vertex_buffer(Util::vector_size_in_bytes(vertices), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
	index_buffer (Util::vector_size_in_bytes(indices),  VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
	sub_meshes(sub_meshes)
{
	VulkanMemory::buffer_copy_staged(vertex_buffer, vertices.data(), Util::vector_size_in_bytes(vertices));
	VulkanMemory::buffer_copy_staged(index_buffer,  indices .data(), Util::vector_size_in_bytes(indices));

	// Sort Submeshes so that Submeshes with the same Texture are contiguous
	std::sort(sub_meshes.begin(), sub_meshes.end(), [](auto const & a, auto const & b) {
		return a.texture_handle < b.texture_handle;
	});
}
