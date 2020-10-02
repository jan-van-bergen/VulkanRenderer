#pragma once
#include "Mesh.h"
#include "Texture.h"

#include "Matrix4.h"

struct Renderable {
	Mesh * mesh;
	u32 texture_index;
	
	Matrix4 transform;
};
