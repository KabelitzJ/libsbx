#ifndef MATERIAL_COMMON_SLANG_
#define MATERIAL_COMMON_SLANG_

struct vertex_setup {
  uint global_vertex;
  uint global_instance;
  float3 world_position;
  float3 world_normal;
  float4 world_tangent;
  float2 uv;
  material_data material;
}; // struct vertex_setup

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

  float sway_speed_strength;
  float scrumble_speed_strength;
  float falloff_exponents;
  uint _pad0;
}; // struct material_data

struct instance_data {
  uint transform_index;
  uint material_index;
  uint object_id; 
  uint payload;
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

  float3 position() {
    return float3(position_x, position_y, position_z);
  }

  float3 normal() {
    return float3(normal_x, normal_y, normal_z);
  }

  float2 uv() {
    return float2(uv_x, uv_y);
  }

  float4 tangent() {
    return float4(tangent_x, tangent_y, tangent_z, tangent_w);
  }

}; // struct vertex

struct push_data {
  vertex* vertex_buffer;
  transform_data* transform_data_buffer;
  material_data* material_data_buffer;
  instance_data* instance_data_buffer;
#if defined(VERTEX_STREAM_COLOR)
  float4* color_buffer;
#endif
#if defined(VERTEX_STREAM_CUSTOM0)
  float4* custom0_buffer;
#endif
#if defined(VERTEX_STREAM_CUSTOM1)
  float4* custom1_buffer;
#endif
#if defined(VERTEX_STREAM_CUSTOM2)
  float4* custom2_buffer;
#endif
#if defined(VERTEX_STREAM_CUSTOM3)
  float4* custom3_buffer;
#endif
}; // struct push_data

#endif // MATERIAL_COMMON_SLANG_
