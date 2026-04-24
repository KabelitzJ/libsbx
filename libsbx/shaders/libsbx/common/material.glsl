#ifndef COMMON_MATERIAL_GLSL
#define COMMON_MATERIAL_GLSL

struct material {
  vec4 albedo;
  vec4 specular;
  float metallic;
  float roughness;
  float ambient_occlusion;
}; // struct material

#endif // COMMON_MATERIAL_GLSL
