#ifndef COMMON_LIGHTING_GLSL
#define COMMON_LIGHTING_GLSL

#include <libsbx/common/material.glsl>

struct point_light {
  vec3 position;
  vec4 color;
  float radius;
}; // point_light

const uint MAX_POINT_LIGHTS = 32;

struct directional_light {
  vec3 direction;
  vec4 color;
}; // directional_light

struct light_result {
  vec4 ambient;
  vec4 diffuse;
  vec4 specular;
}; // struct light_result

// Function to calculate attenuation based on distance and radius
float calculate_attenuation(float distance, float radius) {
  return 1.0 / (1.0 + (distance / radius) * (distance / radius));
}

// Function to calculate lighting from a point light
light_result calculate_point_light(point_light light, vec3 world_position, vec3 normal, vec3 view_position, material material) {
  vec3 light_direction = normalize(light.position - world_position);
  vec3 view_direction = normalize(view_position - world_position);
  vec3 halfway_direction = normalize(light_direction + view_direction);

  // ambient
  float ambient_strength = 0.1;
  vec4 ambient = ambient_strength * light.color;
  
  // diffuse 
  vec3 norm = normalize(normal);
  float diff = max(dot(norm, light_direction), 0.0);
  vec4 diffuse = diff * light.color;
  
  // specular
  float spec = pow(max(dot(normal, halfway_direction), 0.0), 64.0);
  vec4 specular = spec * light.color;  

  return light_result(ambient, diffuse, specular);
}

// Function to calculate lighting from a directional light
light_result calculate_directional_light(directional_light light, vec3 world_position, vec3 normal, vec3 view_position, material material) {
  vec3 light_direction = normalize(-light.direction);
  vec3 view_direction = normalize(view_position - world_position);
  vec3 halfway_direction = normalize(light_direction + view_direction);

  // ambient
  vec4 ambient = material.albedo * light.color * material.ambient_occlusion;

  // diffuse
  vec3 normalized_normal = normalize(normal);
  float diffuse_strength = max(dot(normalized_normal, light_direction), 0.0);
  vec4 diffuse = diffuse_strength * material.albedo * light.color;

  // specular
  float roughness_factor = (1.0 - material.roughness);
  float specular_strength = pow(max(dot(normalized_normal, halfway_direction), 0.0), roughness_factor * 128.0); // assuming 128 as a base shininess
  vec4 specular = specular_strength * mix(vec4(0.04), material.specular, material.metallic) * light.color;

  return light_result(ambient, diffuse, specular);
}

#endif // COMMON_LIGHTING_GLSL
