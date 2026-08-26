// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <common.slang>
#include <frame_data.slang>

#include <clusters/cluster_data.slang>

struct push_data {
  frame_data* frame;
  aabb* clusters;
  uint* counter;
}; // struct push_data

[[vk::push_constant]]
ConstantBuffer<push_data> push;

static const uint max_lights_per_cluster = 128u;
static const uint max_light_indices = 65536u;

struct compute_input {
  uint3 id : SV_DispatchThreadID;
}; // struct compute_input

bool sphere_intersects_aabb(float3 center, float radius, float3 box_min, float3 box_max) {
  float3 closest = clamp(center, box_min, box_max);
  float3 delta = center - closest;
  return dot(delta, delta) <= radius * radius;
}

[shader("compute")]
[numthreads(8, 8, 1)]
void compute_main(compute_input input) {
  uint3 dispatch_thread_id = input.id;

  if (dispatch_thread_id.x >= cluster_dimensions.x || dispatch_thread_id.y >= cluster_dimensions.y || dispatch_thread_id.z >= cluster_dimensions.z) {
    return;
  }

  uint cluster_id = dispatch_thread_id.x + cluster_dimensions.x * (dispatch_thread_id.y + cluster_dimensions.y * dispatch_thread_id.z);

  frame_data frame = push.frame[0];
  aabb aabb = push.clusters[cluster_id];

  uint local_indices[max_lights_per_cluster];
  uint local_count = 0u;

  for (uint i = frame.directional_light_count; i < frame.light_count && local_count < max_lights_per_cluster; ++i) {
    light_data light = frame.lights[i];

    float3 view_position = mul(frame.view, float4(light.position.xyz, 1.0)).xyz;
    float radius = light.position.w;

    if (sphere_intersects_aabb(view_position, radius, aabb.min.xyz, aabb.max.xyz)) {
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
