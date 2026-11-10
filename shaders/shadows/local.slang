// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef SHADOWS_LOCAL_SLANG_
#define SHADOWS_LOCAL_SLANG_

#include <frame_data.slang>
#include <descriptors.slang>
#include <math.slang>
#include <shadows/csm.slang>

// PCF radius in texels for spot and point shadows; filter_margin_texels in scene_renderer_module::_select_local_shadows keeps it inside the tile.
static const float local_shadow_filter_texels = 1.5;

// A point light's cube face for a direction from the light, in the order scene_renderer_module::_select_local_shadows lays its tiles out: +X, -X, +Y, -Y, +Z, -Z.
uint point_shadow_face(float3 direction) {
  float3 magnitude = abs(direction);

  if (magnitude.x >= magnitude.y && magnitude.x >= magnitude.z) {
    return direction.x > 0.0 ? 0u : 1u;
  }

  if (magnitude.y >= magnitude.z) {
    return direction.y > 0.0 ? 2u : 3u;
  }

  return direction.z > 0.0 ? 4u : 5u;
}

// Shadowing of a spot light's tile, or the point light cube face facing world_position, at world_position; 1 = fully lit.
// The receiver is pushed along its normal (normal bias, scaled like csm.slang so 1.0 covers the PCF footprint on a flat surface) and toward the light (depth bias),
// both in texels of the tile at the receiver's distance, since a perspective tile's texels grow with distance.
float calculate_local_shadow(frame_data frame, light_data light, float3 world_position, float3 world_normal, float noise) {
  float3 to_light = light.position.xyz - world_position;

  uint tile = light.type == light_type::point ? light.shadow_index + point_shadow_face(-to_light) : light.shadow_index;
  local_shadow_view view = frame.local_shadow_views[tile];

  float distance = length(to_light);
  float3 l = to_light / max(distance, 0.0001);

  float texel_world = view.bias.z * distance;
  float ndl = dot(world_normal, l);
  float sin_theta = sqrt(saturate(1.0 - ndl * ndl));

  float normal_offset = view.bias.y * (1.0 + local_shadow_filter_texels) * texel_world * sin_theta;
  float depth_offset = view.bias.x * texel_world;

  float4 clip = mul(view.view_projection, float4(world_position + world_normal * normal_offset + l * depth_offset, 1.0));

  if (clip.w <= 0.0) {
    return 1.0;
  }

  float3 ndc = clip.xyz / clip.w;
  float2 uv = shadow_ndc_to_uv(ndc.xy);

  if (any(uv < 0.0) || any(uv > 1.0) || ndc.z < 0.0 || ndc.z > 1.0) {
    return 1.0;
  }

  // One atlas holds every tile, so the descriptor index is uniform; only the tile rect varies per pixel.
  Texture2D atlas = textures[frame.local_shadow_atlas_index];
  SamplerComparisonState shadow_sampler = comparison_samplers[frame.shadow_sampler_index];

  uint width, height;
  atlas.GetDimensions(width, height);

  float2 texel_size = 1.0 / float2(width, height);

  float2 tile_min = view.atlas_rect.xy;
  float2 tile_max = view.atlas_rect.xy + view.atlas_rect.zw;

  // Every tap and its bilinear neighbour stay inside this tile.
  float2 inset = texel_size * 1.5;
  float2 low = tile_min + inset;
  float2 high = tile_max - inset;

  float2 atlas_uv = tile_min + uv * view.atlas_rect.zw;

  float2x2 rotation = shadow_rotation_2d(noise * two_pi);
  uint sample_count = shadow_sample_count();

  float shadow = 0.0;

  for (uint i = 0u; i < sample_count; ++i) {
    float2 offset = mul(rotation, shadow_poisson_sample(i)) * texel_size * local_shadow_filter_texels;
    shadow += atlas.SampleCmpLevelZero(shadow_sampler, clamp(atlas_uv + offset, low, high), ndc.z);
  }

  return shadow / float(sample_count);
}

#endif // SHADOWS_LOCAL_SLANG_
