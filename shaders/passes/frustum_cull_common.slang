// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef FRUSTUM_CULL_COMMON_SLANG_
#define FRUSTUM_CULL_COMMON_SLANG_

#include <frame_data.slang>

// Shared by frustum_cull.slang and frustum_cull_instanced.slang. cascade_index 0xFFFFFFFF = the
// camera frustum, otherwise that shadow cascade's light frustum.

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
    // Shadow casters in front of a cascade's near plane still cast: they're depth-clamped, not clipped.
    if (cascade_index != 0xFFFFFFFFu && i == 4u) {
      continue;
    }

    float4 plane = cascade_index == 0xFFFFFFFFu ? frame.frustum_planes[i] : cascade_frustum_plane(frame.light_view_projections[cascade_index], i);
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

// Field by field: Slang emits a whole-struct store through a physical pointer without the Aligned
// operand, which spirv-val rejects.
void store_transform(transform_data* destination, transform_data value) {
  destination.model = value.model;
  destination.normal = value.normal;
  destination.color = value.color;
  destination.custom_data = value.custom_data;
}

#endif // FRUSTUM_CULL_COMMON_SLANG_
