// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef FRAME_DATA_SLANG_
#define FRAME_DATA_SLANG_

#include <common.slang>
#include <descriptors.slang>

static const uint shadow_cascade_count = 4;

static const uint shader_graph_max_params = 8;
static const uint shader_graph_max_textures = 4;

struct vertex {
  float3 position;
  float3 normal;
  float2 uv;
  float4 tangent;
  float4 color;
}; // struct vertex

struct transform_data {
  float4x4 model;
  float4x4 normal;
}; // struct transform_data

enum class material_flags : uint {
  none = 0,
  alpha_masked = 1 << 0,
  receives_shadow = 1 << 1
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
  float normal_scale;
  float occlusion_strength;
  float emissive_strength;
  float ior;
  float2 uv_tiling;
  float2 uv_offset;
  float4 generic_params[shader_graph_max_params];
  uint generic_textures[shader_graph_max_textures];
}; // struct material_data

float2 apply_uv_transform(material_data material, float2 uv) {
  return uv * material.uv_tiling + material.uv_offset;
}

void apply_alpha_cutout(material_data material, float2 uv, uint sampler_index) {
  float2 transformed_uv = apply_uv_transform(material, uv);
  float alpha = material.base_color_factor.a * textures[material.albedo_index].Sample(samplers[sampler_index], transformed_uv).a;
  uint is_alpha_masked = (material.flags & material_flags::alpha_masked) != 0u;

  clip(is_alpha_masked * (alpha - material.alpha_cutoff));
}

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
  float ambient_intensity;
  uint directional_light_count;
  float cluster_scale;
  range* cluster_ranges;
  uint* cluster_light_indices;
  float cluster_bias;
  float2 cluster_tile_size;
  float4 cascade_splits;
  float4 cascade_depth_bias_per_texel;
  float4x4 light_view_projections[shadow_cascade_count];
  uint shadow_map_indices[shadow_cascade_count];
  uint shadow_enabled;
  float4 frustum_planes[6];
  uint scene_depth_index;
  float near_plane;
  float far_plane;
  float2 render_target_size;
}; // struct frame_data

#endif // FRAME_DATA_SLANG_
