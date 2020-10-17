const float PI = 3.141592653589793238462643383279502884197169f;

struct Material {
	vec3 albedo;
	float roughness;
	float metallic;
};

struct Light {
	vec3 colour;
};

struct DirectionalLight {
	Light base;
	
	vec3 direction;
	
	mat4 light_matrix;
};

struct PointLight {
	Light base;
	
	vec3  position;
	float one_over_radius_squared;
};

struct SpotLight {
	PointLight base;
	
	vec3 direction;
	
	float cutoff_inner;
	float cutoff_outer;
};

float calc_shadow(sampler2D shadow_map, mat4 light_matrix, vec3 world_position) {
	vec2 texel_size = 1.0f / textureSize(shadow_map, 0).xy;

	vec4 shadow_map_coords = light_matrix * vec4(world_position, 1.0f);
	
	// Perspective divide and transform from [-1, 1] to [0, 1]
	shadow_map_coords   /= shadow_map_coords.w;
	shadow_map_coords.xy = shadow_map_coords.xy * 0.5f + 0.5f;
	
	const int range = 1;
	const int total = (2 * range + 1) * (2 * range + 1); // Normalization factor

	float result = 1.0f;

	for (int y = -range; y <= range; y++) {
		for (int x = -range; x <= range; x++) {
			float depth = texture(shadow_map, shadow_map_coords.xy + vec2(x, y) * texel_size).x;
	
			if (depth < shadow_map_coords.z - 0.005f) result -= 1.0f / total;
		}
	}

	return result;
}

float D(float roughness, float n_dot_h) {
	float alpha  = roughness * roughness;
	float alpha2 = alpha * alpha;

	float tmp = n_dot_h * n_dot_h * (alpha2 - 1.0f) + 1.0f;

	return alpha2 / (PI * tmp*tmp); 
}

float G(float roughness, float n_dot_l, float n_dot_v) {
	float k = (roughness + 1.0f) * (roughness + 1.0f) / 8.0;

	float G1_l = n_dot_l / (n_dot_l * (1.0f - k) + k);
	float G1_v = n_dot_v / (n_dot_v * (1.0f - k) + k);

	return G1_l * G1_v;
}

vec3 F(vec3 F_0, float cos_theta) {
	return F_0 + (1.0f - F_0) * pow(1.0f - cos_theta, 5.0f);
}

vec3 brdf(Light light, Material material, vec3 l, vec3 v, vec3 n) {	
	vec3 h = normalize(l + v);

	float n_dot_l = max(dot(n, l), 0.0f);
	float n_dot_v = max(dot(n, v), 0.0f);
	float l_dot_h = max(dot(l, h), 0.0f);
	float n_dot_h = max(dot(n, h), 0.0f);

	if (n_dot_l <= 0.0) return vec3(0.0f);

	float roughness = max(0.05f, material.roughness);

	vec3 F_0 = mix(vec3(0.04f), material.albedo, material.metallic);

	float d = D(roughness, n_dot_h);
	float g = G(roughness, n_dot_l, n_dot_v);
	vec3  f = F(F_0, n_dot_v);

	vec3 diffuse  = material.albedo / PI;
	vec3 specular = d * f * g / (4.0f * n_dot_l * n_dot_v);

	return light.colour * (diffuse + specular) * n_dot_l;
}

vec3 calc_directional_light(DirectionalLight directional_light, Material material, vec3 position, vec3 normal, vec3 camera_position, sampler2D shadow_map) {
	vec3 l = -directional_light.direction;
	vec3 v = normalize(camera_position - position);

	vec3 ambient = 0.1f * material.albedo;

	vec3  light  = brdf(directional_light.base, material, l, v, normal);
	float shadow = calc_shadow(shadow_map, directional_light.light_matrix, position);

	return ambient + light * shadow;
}

vec3 calc_point_light(PointLight point_light, Material material, vec3 position, vec3 normal, vec3 camera_position) {
	vec3 to_light = point_light.position - position;

	float distance_squared = dot(to_light, to_light);

	vec3 l = normalize(to_light);
	vec3 v = normalize(camera_position - position);

	vec3 light = brdf(point_light.base, material, l, v, normal);

	// NOTE: Non-physically based attenuation
	float attenuation = clamp(1.0f - distance_squared * point_light.one_over_radius_squared, 0.0f, 1.0f);

	return light / (4.0f * PI) * attenuation * attenuation;
}

vec3 calc_spot_light(SpotLight spot_light, Material material, vec3 position, vec3 normal, vec3 camera_position) {
	vec3 to_light = spot_light.base.position - position;

	float distance_squared = dot(to_light, to_light);

	vec3 l = normalize(to_light);
	vec3 v = normalize(camera_position - position);

	float spot_factor = dot(l, spot_light.direction);
	if (spot_factor <= 0.0f) return vec3(0.0f);

	vec3 light = brdf(spot_light.base.base, material, l, v, normal);

	// NOTE: Non-physically based attenuation
	float attenuation    = clamp(1.0f - distance_squared * spot_light.base.one_over_radius_squared, 0.0f, 1.0f);
 	float radial_falloff = clamp((spot_factor - spot_light.cutoff_outer) / (spot_light.cutoff_inner - spot_light.cutoff_outer), 0.0f, 1.0f);

	return radial_falloff * light / PI * attenuation * attenuation;
}
