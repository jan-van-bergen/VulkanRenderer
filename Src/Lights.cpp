#include "Lights.h"

#include "Util.h"

namespace IcoSphere {
	static Vector3 vertices[] = { Vector3(0.0f, -1.05f, 0.0f), Vector3(0.446589f, -0.893187f, 0.324462f), Vector3(-0.170578f, -0.893187f, 0.524995f), Vector3(0.759788f, -0.46958f, 0.552012f), Vector3(0.89318f, -0.552023f, 0.0f), Vector3(-0.552016f, -0.893184f, 0.0f), Vector3(-0.170578f, -0.893187f, -0.524995f), Vector3(0.446589f, -0.893187f, -0.324462f), Vector3(0.998611f, 0.0f, 0.324463f), Vector3(-0.290207f, -0.469581f, 0.893182f), Vector3(0.276012f, -0.552024f, 0.849462f), Vector3(0.0f, 0.0f, 1.05f), Vector3(-0.939147f, -0.469576f, 0.0f), Vector3(-0.722599f, -0.552023f, 0.524997f), Vector3(-0.998611f, 0.0f, 0.324463f), Vector3(-0.290207f, -0.469581f, -0.893182f), Vector3(-0.722599f, -0.552023f, -0.524997f), Vector3(-0.617175f, 0.0f, -0.849468f), Vector3(0.759788f, -0.46958f, -0.552012f), Vector3(0.276012f, -0.552024f, -0.849462f), Vector3(0.617175f, 0.0f, -0.849468f), Vector3(0.617175f, 0.0f, 0.849468f), Vector3(-0.617175f, 0.0f, 0.849468f), Vector3(-0.998611f, 0.0f, -0.324463f), Vector3(0.0f, 0.0f, -1.05f), Vector3(0.998611f, 0.0f, -0.324463f), Vector3(0.290207f, 0.469581f, 0.893182f), Vector3(0.722599f, 0.552023f, 0.524997f), Vector3(0.170578f, 0.893187f, 0.524995f), Vector3(-0.759788f, 0.46958f, 0.552012f), Vector3(-0.276012f, 0.552024f, 0.849462f), Vector3(-0.446589f, 0.893187f, 0.324462f), Vector3(-0.759788f, 0.46958f, -0.552012f), Vector3(-0.89318f, 0.552023f, 0.0f), Vector3(-0.446589f, 0.893187f, -0.324462f), Vector3(0.290207f, 0.469581f, -0.893182f), Vector3(-0.276012f, 0.552024f, -0.849462f), Vector3(0.170578f, 0.893187f, -0.524995f), Vector3(0.939147f, 0.469576f, 0.0f), Vector3(0.722599f, 0.552023f, -0.524997f), Vector3(0.552016f, 0.893184f, 0.0f), Vector3(0.0f, 1.05f, 0.0f), };
	static int     indices [] = { 0, 1, 2, 3, 1, 4, 0, 2, 5, 0, 5, 6, 0, 6, 7, 3, 4, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 3, 8, 21, 9, 11, 22, 12, 14, 23, 15, 17, 24, 18, 20, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 40, 37, 41, 40, 39, 37, 39, 35, 37, 37, 34, 41, 37, 36, 34, 36, 32, 34, 34, 31, 41, 34, 33, 31, 33, 29, 31, 31, 28, 41, 31, 30, 28, 30, 26, 28, 28, 40, 41, 28, 27, 40, 27, 38, 40, 25, 39, 38, 25, 20, 39, 20, 35, 39, 24, 36, 35, 24, 17, 36, 17, 32, 36, 23, 33, 32, 23, 14, 33, 14, 29, 33, 22, 30, 29, 22, 11, 30, 11, 26, 30, 21, 27, 26, 21, 8, 27, 8, 38, 27, 20, 24, 35, 20, 19, 24, 19, 15, 24, 17, 23, 32, 17, 16, 23, 16, 12, 23, 14, 22, 29, 14, 13, 22, 13, 9, 22, 11, 21, 26, 11, 10, 21, 10, 3, 21, 8, 25, 38, 8, 4, 25, 4, 18, 25, 7, 19, 18, 7, 6, 19, 6, 15, 19, 6, 16, 15, 6, 5, 16, 5, 12, 16, 5, 13, 12, 5, 2, 13, 2, 9, 13, 4, 7, 18, 4, 1, 7, 1, 0, 7, 2, 10, 9, 2, 1, 10, 1, 3, 10, };
}

void PointLight::init_sphere() {
	auto vertex_buffer_size = sizeof(IcoSphere::vertices);
	auto index_buffer_size  = sizeof(IcoSphere::indices);

	sphere.vertex_buffer = VulkanMemory::buffer_create(vertex_buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	sphere.index_buffer  = VulkanMemory::buffer_create(index_buffer_size,  VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	VulkanMemory::buffer_copy_staged(sphere.vertex_buffer, IcoSphere::vertices, vertex_buffer_size);
	VulkanMemory::buffer_copy_staged(sphere.index_buffer,  IcoSphere::indices,  index_buffer_size);

	sphere.index_count = Util::array_element_count(IcoSphere::indices);
}

void PointLight::free_sphere() {
	VulkanMemory::buffer_free(sphere.vertex_buffer);
	VulkanMemory::buffer_free(sphere.index_buffer);
}
