// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <descriptors.slang>

struct vertex_input {
  uint id : SV_VertexID;
}; // struct vertex_input

struct vertex_output {
  [[vk::location(0)]] float2 uv : TEXCOORD;
  float4 sv_position : SV_Position;
}; // struct vertex_output

struct fragment_input {
  [[vk::location(0)]] float2 uv : TEXCOORD;
}; // struct fragment_input

struct fragment_output {
  float4 color : SV_Target0;
}; // struct fragment_output

struct push_data {
  uint accumulator_index;
  uint revealage_index;
  uint sampler_index;
}; // struct push_data

[[vk::push_constant]] ConstantBuffer<push_data> push;

[shader("vertex")]
vertex_output vertex_main(vertex_input input) {
  float2 uv = float2((input.id << 1) & 2, input.id & 2);

  vertex_output output;

  output.uv = uv;
  output.sv_position = float4(uv * 2.0 - 1.0, 0.0, 1.0);

  return output;
}

[shader("fragment")]
fragment_output fragment_main(fragment_input input) {
  SamplerState sampler = samplers[push.sampler_index];

  float4 accumulator = textures[push.accumulator_index].Sample(sampler, input.uv);
  float revealage = textures[push.revealage_index].Sample(sampler, input.uv).r;

  float3 average = accumulator.rgb / max(accumulator.a, 1e-5);

  fragment_output output;

  output.color = float4(average, 1.0 - revealage);

  return output;
}
