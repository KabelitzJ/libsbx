// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <common.slang>
#include <descriptors.slang>

struct vs_out {
  [[vk::location(0)]] float2 clip : TEXCOORD;
  float4 sv_position : SV_Position;
}; // struct vs_out

struct fs_in {
  [[vk::location(0)]] float2 clip : TEXCOORD;
}; // struct fs_in

struct fs_out {
  float4 color : SV_Target0;
}; // struct fs_out

struct push_data {
  float4x4 inverse_view_projection;
  float4 camera_position;
  uint environment_index;
  uint sampler_index;
  float environment_intensity;
}; // struct push_data

[[vk::push_constant]] ConstantBuffer<push_data> push;

[shader("vertex")]
vs_out vertex_main(uint vertex_id : SV_VertexID) {
  const float2 uv = float2((vertex_id << 1) & 2, vertex_id & 2);
  const float2 clip = uv * 2.0 - 1.0;

  vs_out output;

  output.clip = clip;
  // z = 1 -> far plane, so geometry always wins the depth test
  output.sv_position = float4(clip, 1.0, 1.0);

  return output;
}

[shader("fragment")]
fs_out fragment_main(fs_in input) {
  // Reconstruct the world-space view ray for this pixel from the far-plane clip position.
  float4 world = mul(push.inverse_view_projection, float4(input.clip, 1.0, 1.0));
  world /= world.w;

  float3 direction = normalize(world.xyz - push.camera_position.xyz);

  // Equirectangular lookup.
  const float u = atan2(direction.z, direction.x) / (2.0 * pi) + 0.5;
  const float v = acos(clamp(direction.y, -1.0, 1.0)) / pi;

  float3 color = textures[push.environment_index].SampleLevel(samplers[push.sampler_index], float2(u, v), 0.0).rgb * push.environment_intensity;

  fs_out output;

  output.color = float4(color, 1.0);

  return output;
}
