#ifndef COMMON_SURFACE_SLANG_
#define COMMON_SURFACE_SLANG_

namespace sbx::common {

struct surface {
  float3 world_position;
  float3 world_normal;
  float3 view_direction;
  float3 albedo;
  float3 f0;
  float3 emissive;
  float metallic;
  float roughness;
  float alpha;
  float ndv;
  float specular_strength;
  float ao;
  uint material_flags;
}; // struct surface

} // namespace sbx::common

#endif // COMMON_SURFACE_SLANG_
