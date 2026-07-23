// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

struct vs_out {
  [[vk::location(0)]] float2 uv : UV;
  float4 position : SV_Position;
}; // struct vs_out

struct fs_in {
  [[vk::location(0)]] float2 uv : UV;
}; // struct fs_in

struct fs_out {
  float4 color : SV_Target0;
}; // struct fs_out

struct vertex {
  float3 position;
  float3 normal;
  float2 uv;
}; // struct vertex

struct push_data {
  float4x4 mvp;
  vertex* vertices;
  uint image_index;
  uint sampler_index;
}; // struct push_data

[[vk::push_constant]] ConstantBuffer<push_data> push;

[[vk::binding(0, 0)]] Texture2D textures[];
[[vk::binding(1, 0)]] SamplerState samplers[];

[shader("vertex")]
vs_out vertex_main(uint vertex_id : SV_VertexID) {
  vertex vertex = push.vertices[vertex_id];

  vs_out output;
  output.uv = vertex.uv;
  output.position = mul(push.mvp, float4(vertex.position, 1.0));

  return output;
}

[shader("fragment")]
fs_out fragment_main(fs_in input) {
  fs_out output;

  Texture2D texture = textures[push.image_index];
  SamplerState sampler = samplers[push.sampler_index];

  output.color = texture.Sample(sampler, input.uv);

  return output;
}
