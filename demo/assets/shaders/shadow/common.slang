#ifndef COMMON_SLANG_
#define COMMON_SLANG_

struct transform_data {
  float4x4 model;
  float4x4 normal;
}; // struct transform_data

struct material_data {
  uint albedo_image_index;
  uint albedo_sampler_index;
  uint normal_image_index;
  uint normal_sampler_index;

  uint metallic_roughness_image_index;
  uint metallic_roughness_sampler_index;
  uint occlusion_image_index;
  uint occlusion_sampler_index;

  uint emissive_image_index;
  uint emissive_sampler_index;
  uint height_image_index;
  uint height_sampler_index;

  float height_scale;
  float height_offset;
  float parallax_min_layers;
  float parallax_max_layers;

  float emissive_strength;
  float normal_scale;
  float metallic_factor;
  float roughness_factor;

  float occlusion_strength;
  float specular_factor;
  float alpha_cutoff;
  uint flags;

  float2 uv_offset;
  float2 uv_scale;

  float4 base_color;

  float4 emissive_factor;
}; // struct material_data

struct instance_data {
  uint transform_index;
  uint material_index;
  uint object_id; 
  uint _pad0;
}; // struct instance_data

struct vertex {
  float position_x;
  float position_y;
  float position_z;

  float normal_x;
  float normal_y;
  float normal_z;

  float uv_x;
  float uv_y;

  float tangent_x;
  float tangent_y;
  float tangent_z;
  float tangent_w; // sign of the tangent
}; // struct vertex

struct push_data {
  vertex* vertex_buffer;
  transform_data* transform_data_buffer;
  material_data* material_data_buffer;
  instance_data* instance_data_buffer;
  uint cascade;
}; // struct push_data

#endif // COMMON_SLANG_
