// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <common.slang>

#include <clusters/cluster_data.slang>

struct push_data {
  aabb* clusters;
  float tan_half_fov_y;
  float aspect;
  float near_plane;
  float far_plane;
}; // struct push_data

[[vk::push_constant]]
ConstantBuffer<push_data> push;

struct compute_input {
  uint3 id : SV_DispatchThreadID;
}; // struct compute_input

float3 view_ray(float2 ndc) {
  return float3(ndc.x * push.aspect * push.tan_half_fov_y, -ndc.y * push.tan_half_fov_y, -1.0);
}

[shader("compute")]
[numthreads(8, 8, 1)]
void compute_main(compute_input input) {
  uint3 dispatch_thread_id = input.id;

  if (dispatch_thread_id.x >= cluster_dimensions.x || dispatch_thread_id.y >= cluster_dimensions.y || dispatch_thread_id.z >= cluster_dimensions.z) {
    return;
  }

  uint cluster_id = dispatch_thread_id.x + cluster_dimensions.x * (dispatch_thread_id.y + cluster_dimensions.y * dispatch_thread_id.z);

  float2 ndc_min = (float2(dispatch_thread_id.xy) / float2(cluster_dimensions.x, cluster_dimensions.y)) * 2.0 - 1.0;
  float2 ndc_max = (float2(dispatch_thread_id.xy + uint2(1u, 1u)) / float2(cluster_dimensions.x, cluster_dimensions.y)) * 2.0 - 1.0;

  float3 rays[4] = {
    view_ray(float2(ndc_min.x, ndc_min.y)),
    view_ray(float2(ndc_max.x, ndc_min.y)),
    view_ray(float2(ndc_min.x, ndc_max.y)),
    view_ray(float2(ndc_max.x, ndc_max.y))
  };

  float slice_ratio = push.far_plane / push.near_plane;
  float near_z = push.near_plane * pow(slice_ratio, float(dispatch_thread_id.z) / float(cluster_dimensions.z));
  float far_z = push.near_plane * pow(slice_ratio, float(dispatch_thread_id.z + 1u) / float(cluster_dimensions.z));

  float3 aabb_min = float3(3.402823466e+38, 3.402823466e+38, 3.402823466e+38);
  float3 aabb_max = -aabb_min;

  for (uint i = 0u; i < 4u; ++i) {
    float3 near_point = rays[i] * near_z;
    float3 far_point = rays[i] * far_z;

    aabb_min = min(aabb_min, min(near_point, far_point));
    aabb_max = max(aabb_max, max(near_point, far_point));
  }

  push.clusters[cluster_id].min = float4(aabb_min, 1.0);
  push.clusters[cluster_id].max = float4(aabb_max, 1.0);
}
