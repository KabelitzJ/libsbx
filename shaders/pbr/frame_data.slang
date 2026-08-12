#ifndef FRAME_DATA_SLANG_
#define FRAME_DATA_SLANG_

struct vertex {
  float3 position;
  float3 normal;
  float2 uv;
  float4 tangent;
}; // struct vertex

enum class material_flags : uint {
  none = 0,
  alpha_masked = 1 << 0
}; // enum material_flags

struct material_data {
  float4 base_color_factor;
  float4 emissive_factor;
  uint albedo_index;
  uint normal_index;
  uint metallic_roughness_index;
  uint occlusion_index;
  uint emissive_index;
  float metallic_factor;
  float roughness_factor;
  float alpha_cutoff;
  uint flags;
  uint pad0;
  uint pad1;
  uint pad2;
}; // struct material_data

enum class light_type : uint {
  directional = 0,
  point = 1,
  spot = 2
}; // enum light_type

struct light_data {
  float4 color;
  float4 position;
  float4 direction;
  light_type type;
  float inner_cos;
  float outer_cos;
  uint padding;
}; // struct light_data

struct frame_data {
  float4x4 view;
  float4x4 projection;
  float4 camera_position;
  light_data* lights;
  uint light_count;
  uint padding;
  material_data* materials;
  uint irradiance_index;
  uint brdf_lut_index;
  uint prefiltered_index;
  uint prefiltered_mip_count;
  float environment_intensity;
  uint pad0;
}; // struct frame_data

#endif // FRAME_DATA_SLANG_
