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
	
	shadow_map_coords /= shadow_map_coords.w;
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

vec4 calc_light(Light light, vec3 position, vec3 direction, vec3 normal, vec3 camera_position) {
    vec4 result = vec4(0.0f, 0.0f, 0.0f, 1.0f);
	
    float diffuse = dot(normal, -direction);
    
    if (diffuse > 0.0f) {
        result.rgb = light.colour * diffuse;

		// Calculate specular lighting (Blinn-Phong method)
        vec3 to_eye     = normalize(camera_position - position);
        vec3 half_angle = normalize(to_eye - direction);

        const float specular_power     = 32.0f;
        const float specular_intensity =  1.0f;

        float specular = dot(normal, half_angle);
        specular       = pow(specular, specular_power);

        result.rgb = light.colour * (diffuse + specular_intensity * specular);
    }

    return result;
}

vec4 calc_directional_light(DirectionalLight directional_light, vec3 position, vec3 normal, vec3 camera_position, sampler2D shadow_map) {
    return 
		calc_shadow(shadow_map, directional_light.light_matrix, position) *
		calc_light(directional_light.base, position, directional_light.direction, normal, camera_position);
}

vec4 calc_point_light(PointLight point_light, vec3 position, vec3 normal, vec3 camera_position) {
    vec3 light_direction = position - point_light.position;
    
    vec4 colour = calc_light(point_light.base, position, normalize(light_direction), normal, camera_position);
    
    // Attenuation based on pointlight's range
	float distance_squared = dot(light_direction, light_direction);
	
    float attenuation = clamp(1.0f - distance_squared * point_light.one_over_radius_squared, 0.0f, 1.0f);

    return colour * attenuation * attenuation;
}

vec4 calc_spot_light(SpotLight spot_light, vec3 position, vec3 normal, vec3 camera_position) {
	vec3 light_direction = position - spot_light.base.position;
	
	float spot_factor = dot(normalize(light_direction), spot_light.direction);
	
	if (spot_factor <= 0.0f) return vec4(0.0f);
	
	float radial_falloff = clamp((spot_factor - spot_light.cutoff_outer) / (spot_light.cutoff_inner - spot_light.cutoff_outer), 0.0f, 1.0f);
	
	return radial_falloff * calc_point_light(spot_light.base, position, normal, camera_position);
}
