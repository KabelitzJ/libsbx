// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <descriptors.slang>

struct vs_in {
  uint id : SV_VertexID;
}; // struct vs_in

struct vs_out {
  [[vk::location(0)]] float2 uv : TEXCOORD;
  float4 sv_position : SV_Position;
}; // struct vs_out

struct fs_in {
  [[vk::location(0)]] float2 uv : TEXCOORD;
}; // struct fs_in

struct fs_out {
  float4 color : SV_Target0;
}; // struct fs_out

struct push_data {
  uint accum_index;
  uint reveal_index;
  uint sampler_index;
}; // struct push_data

[[vk::push_constant]] ConstantBuffer<push_data> push;

[shader("vertex")]
vs_out vertex_main(vs_in input) {
  float2 uv = float2((input.id << 1) & 2, input.id & 2);

  vs_out output;

  output.uv = uv;
  output.sv_position = float4(uv * 2.0 - 1.0, 0.0, 1.0);

  return output;
}

[shader("fragment")]
fs_out fragment_main(fs_in input) {
  SamplerState sampler = samplers[push.sampler_index];

  float4 accumulator = textures[push.accum_index].Sample(sampler, input.uv);
  float revealage = textures[push.reveal_index].Sample(sampler, input.uv).r;

  float3 average = accumulator.rgb / max(accumulator.a, 1e-5);

  fs_out output;

  output.color = float4(average, 1.0 - revealage);

  return output;
}
