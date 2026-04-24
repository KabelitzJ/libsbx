#ifndef COMMON_SHADOW_GLSL_
#define COMMON_SHADOW_GLSL_

#include <libsbx/common/random.glsl>

const int PCF_COUNT = 4;
const float RADIUS = 0.0004;
const float BIAS = 0.000025;
const float SAMPLE_WEIGHT = 1.0 / (PCF_COUNT * PCF_COUNT);
const vec2 OFFSET = vec2(0.5 - 0.5 * PCF_COUNT);

float calculate_shadow_random_jitter(sampler2D shadow_map, vec4 light_space_position, vec3 normal, vec3 light_direction) {
  vec3 coordinates = light_space_position.xyz / light_space_position.w;

  if (coordinates.z > 1.0 || coordinates.z < -1.0) {
    return 0.0;
  }

  const float current_depth = coordinates.z - BIAS;

	float shadow_factor = 0.0;

	for(int x = 0; x < PCF_COUNT; x++) {
    for(int y = 0; y < PCF_COUNT; y++) {
      vec2 sample_position = vec2(x, y) + OFFSET;
      vec2 jitter = random_2d(gl_FragCoord.xy + vec2(13278 * x, 321 * y));
      float object_depth = texture(shadow_map, coordinates.xy + (sample_position + jitter) * RADIUS).r;

      shadow_factor += current_depth > object_depth ? 0.0 : SAMPLE_WEIGHT;
    }
	}

	return shadow_factor;
}

float calculate_shadow_pcf(sampler2D shadow_map, vec4 light_space_position, vec3 normal, vec3 light_direction) {
  // perform perspective divide
  vec3 projected_coords = light_space_position.xyz / light_space_position.w;

  if(projected_coords.z > 1.0) {
    return 0.0;
  }

  // check whether current frag pos is in shadow
  
  vec2 size = 1.0 / textureSize(shadow_map, 0);

  float shadow_factor = 0.0;

  int count = PCF_COUNT / 2;
  int iterations = 0;

  for(int y = -count; y <= count; ++y) {
    for(int x = -count; x <= count; ++x) {
      float depth = texture(shadow_map, projected_coords.xy + vec2(x, y) * size).r; 

      shadow_factor += (projected_coords.z - BIAS) > depth ? 0.0 : 1.0;
      iterations++;    
    }    
  }

  return shadow_factor / iterations;
}

float calculate_shadow_pcf_gaussian(sampler2D shadow_map, vec4 light_space_position, vec3 normal, vec3 light_direction) {
  vec3 projected_coords = light_space_position.xyz / light_space_position.w;

  if(projected_coords.z > 1.0) {
    return 0.0;
  }

  vec2 shadow_uv = projected_coords.xy;
  vec2 size = 1.0 / textureSize(shadow_map, 0);

  float current_depth = projected_coords.z - BIAS;

  float kernel[9] = float[](
    0.077847, 0.123317, 0.077847,
    0.123317, 0.195346, 0.123317,
    0.077847, 0.123317, 0.077847
  );

  vec2 offsets[9] = vec2[](
    vec2(-1.0, -1.0), vec2(0.0, -1.0), vec2(1.0, -1.0),
    vec2(-1.0, 0.0), vec2(0.0, 0.0), vec2(1.0, 0.0),
    vec2(-1.0, 1.0), vec2(0.0, 1.0), vec2(1.0, 1.0)
  );

  float shadow_factor = 0.0;

  for(int i = 0; i < 9; i++) {
    vec2 offset = offsets[i] * size;

    float depth = texture(shadow_map, shadow_uv + offset).r;

    shadow_factor += current_depth > depth ? 0.0 : kernel[i];
  }

  return shadow_factor;
}

#endif // COMMON_SHADOW_GLSL_
