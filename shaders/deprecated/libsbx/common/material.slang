#ifndef COMMON_MATERIAL_SLANG_
#define COMMON_MATERIAL_SLANG_

namespace sbx::common {

struct material {
  float4 albedo;
  float4 specular;
  float metallic;
  float roughness;
  float ambient_occlusion;
}; // struct material

enum material_feature : uint {
  none = 0,
  cast_shadow = (1 << 0),
  receive_shadow = (1 << 1),
  invert_backface_normals = (1 << 2),
}; // enum material_feature
 
bool has_feature(uint flags, material_feature feature) {
  return (flags & (uint)feature) != 0;
}

} // namespace sbx::common

#endif // COMMON_MATERIAL_SLANG_
