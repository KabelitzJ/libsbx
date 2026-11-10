// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef FRUSTUM_CULL_COMMON_SLANG_
#define FRUSTUM_CULL_COMMON_SLANG_

#include <frame_data.slang>

// Shared by frustum_cull.slang and frustum_cull_instanced.slang. cascade_index 0xFFFFFFFF = the
// camera frustum, otherwise that shadow view's light frustum (a sun cascade or a local light tile).

float4 cascade_frustum_plane(float4x4 m, uint i) {
  switch (i) {
    case 0u: return m[3] + m[0];
    case 1u: return m[3] - m[0];
    case 2u: return m[3] + m[1];
    case 3u: return m[3] - m[1];
    case 4u: return m[2];
    default: return m[3] - m[2];
  }
}

bool aabb_in_frustum(frame_data frame, uint cascade_index, float3 center, float3 extent) {
  for (uint i = 0u; i < 6u; ++i) {
    // Shadow casters in front of a shadow view's near plane still cast: they're depth-clamped, not clipped.
    if (cascade_index != 0xFFFFFFFFu && i == 4u) {
      continue;
    }

    float4 plane = cascade_index == 0xFFFFFFFFu ? frame.frustum_planes[i] : cascade_frustum_plane(shadow_view_projection(frame, cascade_index), i);
    float radius = extent.x * abs(plane.x) + extent.y * abs(plane.y) + extent.z * abs(plane.z);
    float distance = dot(plane.xyz, center) + plane.w;

    if (distance + radius < 0.0) {
      return false;
    }
  }

  return true;
}

// Whether a local-space box, placed by model, touches the view.
bool instance_visible(frame_data frame, uint cascade_index, float4x4 model, float3 local_min, float3 local_max) {
  float3 world_min = float3(3.402823466e+38, 3.402823466e+38, 3.402823466e+38);
  float3 world_max = -world_min;

  for (uint i = 0u; i < 8u; ++i) {
    float3 corner = float3((i & 4u) != 0u ? local_max.x : local_min.x, (i & 2u) != 0u ? local_max.y : local_min.y, (i & 1u) != 0u ? local_max.z : local_min.z);
    float3 world_corner = mul(model, float4(corner, 1.0)).xyz;
    world_min = min(world_min, world_corner);
    world_max = max(world_max, world_corner);
  }

  return aabb_in_frustum(frame, cascade_index, (world_min + world_max) * 0.5, (world_max - world_min) * 0.5);
}

// Detail levels a command can have (render_pass.hpp's max_lod_levels): the submesh and up to 4 simplifications.
static const uint max_lod_levels = 5u;

// The last fifth of the way to each switch crossfades the two levels, so no level ever pops in.
static const float lod_transition_band = 0.2;

struct lod_choice {
  uint level;
  float next_fade; // the next coarser level's share of the crossfade, 0 when not fading
}; // struct lod_choice

// The coarsest level whose simplification error stays under the view's pixel threshold, judged from the camera for every view.
// meshopt's errors are relative to the mesh's extent, so they're scaled by the instance's world-space diameter.
lod_choice choose_lod(frame_data frame, uint cascade_index, float4x4 model, float3 local_min, float3 local_max, uint lod_count, float4 lod_errors) {
  lod_choice result;
  result.level = 0u;
  result.next_fade = 0.0;

  if (lod_count <= 1u) {
    return result;
  }

  float3 world_center = mul(model, float4((local_min + local_max) * 0.5, 1.0)).xyz;
  float scale = max(length(mul(model, float4(1.0, 0.0, 0.0, 0.0)).xyz), max(length(mul(model, float4(0.0, 1.0, 0.0, 0.0)).xyz), length(mul(model, float4(0.0, 0.0, 1.0, 0.0)).xyz)));
  float radius = length(local_max - local_min) * 0.5 * scale;

  // From the nearest point of the bounding sphere, so a large mesh the camera is next to stays detailed.
  float distance = max(length(world_center - frame.camera_position.xyz) - radius, 0.0001);
  float pixels_per_unit = frame.lod_projection_scale / distance;
  float threshold = cascade_index == 0xFFFFFFFFu ? frame.lod_error_threshold : frame.shadow_lod_error_threshold;

  for (uint level = 1u; level < lod_count; ++level) {
    float pixels = lod_errors[level - 1u] * 2.0 * radius * pixels_per_unit;

    if (pixels <= threshold) {
      result.level = level;
      continue;
    }

    // Not acceptable yet: fade toward it over the last band before it becomes acceptable.
    float progress = threshold / pixels;
    float band_start = 1.0 - lod_transition_band;

    if (progress > band_start) {
      result.next_fade = (progress - band_start) / lod_transition_band;
    }

    break;
  }

  return result;
}

// Camera-view occlusion modes (the push constant's occlusion_mode):
// none: shadow views, or occlusion culling off: frustum test only.
// early: draws what was visible last frame, so the depth pyramid can be built from it.
// late: tests every frustum-visible instance against that pyramid, records the result for next frame, and draws what's visible but wasn't drawn early.
static const uint occlusion_mode_none = 0u;
static const uint occlusion_mode_early = 1u;
static const uint occlusion_mode_late = 2u;

// Whether a local box placed by model is entirely behind the depth pyramid (non-reversed depth: 0 near, 1 far).
// The pyramid's texels hold the farthest depth they cover, so a box whose nearest point is farther than that is hidden everywhere it could show.
bool instance_occluded(frame_data frame, float4x4 model, float3 local_min, float3 local_max) {
  float4x4 view_projection = mul(frame.projection, frame.view);

  float2 uv_min = float2(1.0, 1.0);
  float2 uv_max = float2(0.0, 0.0);
  float nearest = 1.0;

  for (uint i = 0u; i < 8u; ++i) {
    float3 corner = float3((i & 4u) != 0u ? local_max.x : local_min.x, (i & 2u) != 0u ? local_max.y : local_min.y, (i & 1u) != 0u ? local_max.z : local_min.z);
    float4 clip = mul(view_projection, mul(model, float4(corner, 1.0)));

    // Straddling the near plane: can't be projected safely, and it's close enough to matter.
    if (clip.w <= 0.0001) {
      return false;
    }

    float3 ndc = clip.xyz / clip.w;
    float2 uv = ndc.xy * 0.5 + 0.5;

    uv_min = min(uv_min, uv);
    uv_max = max(uv_max, uv);
    nearest = min(nearest, ndc.z);
  }

  uv_min = saturate(uv_min);
  uv_max = saturate(uv_max);

  float2 size = float2(frame.hiz_width, frame.hiz_height);
  float2 texel_min = uv_min * size;
  float2 texel_max = uv_max * size;

  // The level where the box spans at most two texels per axis; each level's texel covers 2^level level-0 texels.
  float span = max(texel_max.x - texel_min.x, texel_max.y - texel_min.y);
  uint level = min(uint(max(ceil(log2(max(span, 1.0))), 0.0)), frame.hiz_mip_count - 1u);

  int2 level_size = max(int2(int(frame.hiz_width) >> level, int(frame.hiz_height) >> level), int2(1, 1));
  int2 low = clamp(int2(floor(texel_min / float(1u << level))), int2(0, 0), level_size - 1);
  int2 high = clamp(int2(floor(texel_max / float(1u << level))), int2(0, 0), level_size - 1);

  Texture2D pyramid = textures[frame.hiz_index];

  float farthest = max(max(pyramid.Load(int3(low.x, low.y, int(level))).r, pyramid.Load(int3(high.x, low.y, int(level))).r), max(pyramid.Load(int3(low.x, high.y, int(level))).r, pyramid.Load(int3(high.x, high.y, int(level))).r));

  return nearest > farthest;
}

// Field by field: Slang emits a whole-struct store through a physical pointer without the Aligned
// operand, which spirv-val rejects.
void store_transform(transform_data* destination, transform_data value) {
  destination.model = value.model;
  destination.normal = value.normal;
  destination.color = value.color;
  destination.custom_data = value.custom_data;
}

#endif // FRUSTUM_CULL_COMMON_SLANG_
