#pragma once

struct Material {
	float roughness = 0.9f;
	float metallic  = 0.0f;

	Material(float roughness, float metallic) : roughness(roughness), metallic(metallic) { }
};
