// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
//
// Clustered Forward+, step 2 of 2: assigns every point/spot light to the clusters its bounding
// sphere overlaps. One thread per cluster, same grid as build_clusters.slang. Each thread first
// tests every point/spot light locally (no atomics needed for that), then reserves its slice of
// the global light-index list with a single atomic add sized to exactly how many lights it found,
// and finally writes its indices into that slice — the same "count locally, reserve once, write"
// shape frustum_culling/compute.slang uses for instance compaction, applied to lights instead.
// Directional lights (lights[0, directional_light_count)) are skipped entirely here — they're
// screen-wide and stay in the always-evaluated prefix geometry.slang reads directly.

#include <frame_data.slang>

struct push_data {
  frame_data* frame;
  cluster_aabb* clusters;
  uint* counter;
}; // struct push_data

[[vk::push_constant]]
ConstantBuffer<push_data> push;

// Per-thread scratch capacity before the atomic reserve — generous relative to how many lights
// are expected to overlap one cluster in practice. A cluster that finds more than this simply
// drops the excess rather than overflowing; worth revisiting once real scenes exercise this.
static const uint max_lights_per_cluster = 128u;

// Sized to comfortably outlive light_capacity (256) worth of lights spread across every cluster in
// a typical scene; see render_module::cluster_light_index_capacity, which this must match.
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

    // Spot lights are conservatively bounded by the same sphere as a point light of equal range —
    // over-inclusive (a spot's actual cone is smaller than its bounding sphere), never
    // under-inclusive, which is the safe direction for a broad-phase culling test.
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
