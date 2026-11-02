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
static const uint max_decals_per_cluster = 32u;
static const uint max_probes_per_cluster = 8u;

// Mirrors scene_renderer_module::cluster_index_capacity.
static const uint max_cluster_indices = 131072u;

struct compute_input {
  uint3 id : SV_DispatchThreadID;
}; // struct compute_input

bool sphere_intersects_aabb(float3 center, float radius, float3 box_min, float3 box_max) {
  float3 closest = clamp(center, box_min, box_max);
  float3 delta = center - closest;
  return dot(delta, delta) <= radius * radius;
}

// Separating-axis test of an oriented box (center and three orthogonal half extent vectors) against an axis-aligned one, on the six face normals.
// Skipping the nine edge-edge axes can only report an overlap that isn't there, which costs a wasted test in shading, never a missing decal.
bool obb_intersects_aabb(float3 center, float3 axis_x, float3 axis_y, float3 axis_z, float3 box_min, float3 box_max) {
  float3 box_center = (box_min + box_max) * 0.5;
  float3 box_half = (box_max - box_min) * 0.5;
  float3 delta = center - box_center;

  float3 obb_radius = abs(axis_x) + abs(axis_y) + abs(axis_z);

  if (any(abs(delta) > box_half + obb_radius)) {
    return false;
  }

  // Projected onto an unnormalized axis everything scales by its length, so the box's own radius along it is its squared length.
  float3 axes[3] = { axis_x, axis_y, axis_z };

  for (uint i = 0u; i < 3u; ++i) {
    float3 axis = axes[i];

    if (abs(dot(delta, axis)) > dot(axis, axis) + dot(box_half, abs(axis))) {
      return false;
    }
  }

  return true;
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

  uint light_indices[max_lights_per_cluster];
  uint light_count = 0u;

  for (uint i = frame.directional_light_count; i < frame.light_count && light_count < max_lights_per_cluster; ++i) {
    light_data light = frame.lights[i];

    float3 view_position = mul(frame.view, float4(light.position.xyz, 1.0)).xyz;
    float radius = light.position.w;

    if (sphere_intersects_aabb(view_position, radius, aabb.min.xyz, aabb.max.xyz)) {
      light_indices[light_count] = i;
      ++light_count;
    }
  }

  // In sort order, so shading draws later decals over earlier ones.
  uint decal_indices[max_decals_per_cluster];
  uint decal_count = 0u;

  for (uint i = 0u; i < frame.decal_count && decal_count < max_decals_per_cluster; ++i) {
    decal_data decal = frame.decals[i];

    float3 center = mul(frame.view, float4(decal.center.xyz, 1.0)).xyz;
    float3 axis_x = mul(frame.view, float4(decal.axis_x.xyz, 0.0)).xyz;
    float3 axis_y = mul(frame.view, float4(decal.axis_y.xyz, 0.0)).xyz;
    float3 axis_z = mul(frame.view, float4(decal.axis_z.xyz, 0.0)).xyz;

    if (obb_intersects_aabb(center, axis_x, axis_y, axis_z, aabb.min.xyz, aabb.max.xyz)) {
      decal_indices[decal_count] = i;
      ++decal_count;
    }
  }

  // Most important first, so shading blends the best two that cover a pixel.
  uint probe_indices[max_probes_per_cluster];
  uint probe_count = 0u;

  for (uint i = 0u; i < frame.probe_count && probe_count < max_probes_per_cluster; ++i) {
    probe_data probe = frame.probes[i];

    float3 center = mul(frame.view, float4(probe.center.xyz, 1.0)).xyz;
    float3 axis_x = mul(frame.view, float4(probe.axis_x.xyz, 0.0)).xyz;
    float3 axis_y = mul(frame.view, float4(probe.axis_y.xyz, 0.0)).xyz;
    float3 axis_z = mul(frame.view, float4(probe.axis_z.xyz, 0.0)).xyz;

    if (obb_intersects_aabb(center, axis_x, axis_y, axis_z, aabb.min.xyz, aabb.max.xyz)) {
      probe_indices[probe_count] = i;
      ++probe_count;
    }
  }

  uint total = light_count + decal_count + probe_count;
  uint base_offset = 0u;

  if (total > 0u) {
    InterlockedAdd(push.counter[0], total, base_offset);

    // Out of room: keep what fits, lights first, then decals, then probes.
    uint room = base_offset < max_cluster_indices ? max_cluster_indices - base_offset : 0u;

    light_count = min(light_count, room);
    decal_count = min(decal_count, room - light_count);
    probe_count = min(probe_count, room - light_count - decal_count);

    for (uint j = 0u; j < light_count; ++j) {
      frame.cluster_indices[base_offset + j] = light_indices[j];
    }

    for (uint j = 0u; j < decal_count; ++j) {
      frame.cluster_indices[base_offset + light_count + j] = decal_indices[j];
    }

    for (uint j = 0u; j < probe_count; ++j) {
      frame.cluster_indices[base_offset + light_count + decal_count + j] = probe_indices[j];
    }
  }

  frame.cluster_ranges[cluster_id].offset = base_offset;
  frame.cluster_ranges[cluster_id].light_count = light_count;
  frame.cluster_ranges[cluster_id].decal_count = decal_count;
  frame.cluster_ranges[cluster_id].probe_count = probe_count;
}
