// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <frame_data.slang>

struct push_data {
  frame_data* frame;
  cluster_aabb* clusters;
  uint* counter;
}; // struct push_data

[[vk::push_constant]]
ConstantBuffer<push_data> push;

static const uint max_lights_per_cluster = 128u;
static const uint max_light_indices = 65536u;

struct cs_in {
  uint3 id : SV_DispatchThreadID;
}; // struct cs_in

bool sphere_intersects_aabb(float3 center, float radius, float3 box_min, float3 box_max) {
  float3 closest = clamp(center, box_min, box_max);
  float3 delta = center - closest;
  return dot(delta, delta) <= radius * radius;
}

[shader("compute")]
[numthreads(8, 8, 1)]
void compute_main(cs_in input) {
  uint3 dispatch_thread_id = input.id;

  if (dispatch_thread_id.x >= cluster_dim_x || dispatch_thread_id.y >= cluster_dim_y || dispatch_thread_id.z >= cluster_dim_z) {
    return;
  }

  uint cluster_id = dispatch_thread_id.x + cluster_dim_x * (dispatch_thread_id.y + cluster_dim_y * dispatch_thread_id.z);

  frame_data frame = push.frame[0];
  cluster_aabb aabb = push.clusters[cluster_id];

  uint local_indices[max_lights_per_cluster];
  uint local_count = 0u;

  for (uint i = frame.directional_light_count; i < frame.light_count && local_count < max_lights_per_cluster; ++i) {
    light_data light = frame.lights[i];

    float3 view_position = mul(frame.view, float4(light.position.xyz, 1.0)).xyz;
    float radius = light.position.w;

    if (sphere_intersects_aabb(view_position, radius, aabb.min_view.xyz, aabb.max_view.xyz)) {
      local_indices[local_count] = i;
      ++local_count;
    }
  }

  uint base_offset = 0u;

  if (local_count > 0u) {
    InterlockedAdd(push.counter[0], local_count, base_offset);

    if (base_offset >= max_light_indices) {
      local_count = 0u;
    } else if (base_offset + local_count > max_light_indices) {
      local_count = max_light_indices - base_offset;
    }

    for (uint j = 0u; j < local_count; ++j) {
      frame.cluster_light_indices[base_offset + j] = local_indices[j];
    }
  }

  frame.cluster_ranges[cluster_id].offset = base_offset;
  frame.cluster_ranges[cluster_id].count = local_count;
}
