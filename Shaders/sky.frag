#version 450

layout(location = 0) in vec2 in_uv;

layout(location = 0) out vec4 out_colour;

layout(binding = 0) uniform UBO {
	vec3 camera_top_left_corner;
	vec3 camera_x;
	vec3 camera_y;

	vec3 sun_direction;
};

// Based on: https://github.com/SimonWallner/kocmoc-demo/blob/RTVIS/media/shaders/scattering.glsl

const float PI = 3.141592653589793238462643383279502884197169f;

const float cutoff_angle = PI / 1.95f;
const float steepness = 1.5;

const float zenith_length_rayleigh = 8.4e3;
const float zenith_length_mie      = 1.25e3;

const float sun_intensity = 1000.0f;

const float mie_coefficient   = 0.005f;
const float mie_directional_g = 0.80f;

vec3 total_mie() {
	const vec3 primary_wavelengths = { 680e-9f, 550e-9f, 450e-9f };
	const vec3 K                   = { 0.686f, 0.678f, 0.666f };
	const float turbidity = 1.0f;

	const float c = (0.2f * turbidity) * 10e-18f;

	vec3 a = (2.0f * PI) / primary_wavelengths;

	return 0.434f * c * PI * a * a * K;
}

float phase_rayleigh(float cos_angle_view_sun) {
	return (3.0f / (16.0f * PI)) * (1.0f + cos_angle_view_sun * cos_angle_view_sun);
}

float phase_henyey_greenstein(float cos_angle_view_sun, float g) {
	return (1.0f / (4.0f * PI)) * ((1.0f - g * g) / pow(1.0f - 2.0f * g * cos_angle_view_sun + g * g, 1.5f));
}

void main(void) {
	vec3 direction = normalize(camera_top_left_corner +
		in_uv.x * camera_x +
		in_uv.y * camera_y
	);

	const vec3 up = { 0.0f, 1.0f , 0.0f };

	float cos_angle_view_sun = dot(direction, sun_direction);
	float cos_angle_up_view  = dot(direction,     up);
	float cos_angle_sun_up   = dot(sun_direction, up);

	float zenith_angle = max(0.0f, cos_angle_up_view);

	const vec3 x_rayleigh = vec3(5.176821E-6f, 1.2785348E-5f, 2.8530756E-5f);
	const vec3 x_mie      = total_mie() * mie_coefficient;

	float optical_length_rayleigh = zenith_length_rayleigh / zenith_angle;
	float optical_length_mie      = zenith_length_mie      / zenith_angle;

	vec3 extinction_factor = exp(-(x_rayleigh * optical_length_rayleigh + x_mie * optical_length_mie));

	// In-scattering
	vec3 to_eye_rayleigh = x_rayleigh * phase_rayleigh(cos_angle_view_sun);
	vec3 to_eye_mie      = x_mie      * phase_henyey_greenstein(cos_angle_view_sun, mie_directional_g);

	vec3 sum_x      = x_rayleigh + x_mie;
	vec3 sum_to_eye = to_eye_rayleigh + to_eye_mie;

	vec3 sun = sun_intensity * max(0.0f, 1.0f - exp(-((cutoff_angle - acos(cos_angle_sun_up)) / steepness))) * (sum_to_eye / sum_x);

	float t = clamp(pow(1.0f - dot(up, sun_direction), 5.0f), 0.0f, 1.0f);

	vec3 sky = sun * (1.0f - extinction_factor) * mix(vec3(1.0f), sqrt(sun * extinction_factor), t);

	out_colour = vec4(0.1f * sky, 1.0f);
}