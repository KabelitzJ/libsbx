#ifndef COMMON_WIND_GLSL_
#define COMMON_WIND_GLSL_

#include <libsbx/common/constants.glsl>

vec3 wind_effect(vec3 world_position, vec3 position, float time, float flexibility, float anchor_height, float max_anchor_height){
	float height_from_anchor = max(0.0, position.y - (anchor_height * max_anchor_height));

	float amplitude = height_from_anchor * flexibility;

	float wave = sin(2.0 * PI * time + (world_position.z + world_position.x) * 0.8);
	float wave2 = sin(2.0 * PI * (time + world_position.z + world_position.x) * 2.0);

	world_position.x += (wave + wave2 * 0.4) * 0.06 * amplitude;
	world_position.z -= (wave - wave2 * 0.4) * 0.03 * amplitude;

	return world_position;
}

#endif // COMMON_WIND_GLSL_
