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

  uint mrao_image_index;
  uint mrao_sampler_index;
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

  float metallic;
  float roughness;
  float occlusion;
  float alpha_cutoff;

  float2 uv_offset;
  float2 uv_scale;

  float4 base_color;

  float4 emissive_factor;

  uint flags;
  uint _pad0;
  uint _pad1;
  uint _pad2;
}; // struct material_data

struct static_instance_data {
  uint transform_index;
  uint material_index;
  uint object_id; 
  uint _pad0;
}; // struct instance_data

struct skinned_instance_data {
  uint transform_index;
  uint material_index;
  uint object_id;
  uint bone_matrix_offset;
}; // struct instance_data

struct static_vertex {
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
}; // struct static_vertex

struct skinned_vertex {
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

  uint bone_index_x;
  uint bone_index_y;
  uint bone_index_z;
  uint bone_index_w;

  float bone_weight_x;
  float bone_weight_y;
  float bone_weight_z;
  float bone_weight_w;
}; // struct skinned_vertex

struct static_push_data {
  static_vertex* vertex_buffer;
  transform_data* transform_data_buffer;
  material_data* material_data_buffer;
  static_instance_data* instance_data_buffer;
}; // struct push_data

struct skinned_push_data {
  skinned_vertex* vertex_buffer;
  transform_data* transform_data_buffer;
  material_data* material_data_buffer;
  skinned_instance_data* instance_data_buffer;
  float4x4* bone_matrices_buffer;
  uint bone_to_track;
}; // struct push_data

#endif // COMMON_SLANG_
