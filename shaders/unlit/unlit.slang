// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

struct vs_in {
  [[vk::location(0)]] uint vertex_id : SV_VertexID;
  [[vk::location(1)]] uint instance_id : SV_InstanceID;
}; // struct vs_in

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

struct material_data {
  float4 base_color_factor;
  uint albedo_index;
  uint padding0;
  uint padding1;
  uint padding2;
}; // struct material_data

struct frame_data {
  float4x4 view;
  float4x4 projection;
  float4 camera_position;
  material_data* materials;
}; // struct frame_data

struct push_data {
  frame_data* frame;
  vertex* vertices;
  float4x4 model;
  uint material_index;
  uint sampler_index;
}; // struct push_data

[[vk::push_constant]] ConstantBuffer<push_data> push;

[[vk::binding(0, 0)]] Texture2D textures[];
[[vk::binding(1, 0)]] SamplerState samplers[];

[shader("vertex")]
vs_out vertex_main(vs_in input) {
  
  vertex vertex = push.vertices[input.vertex_id];
  frame_data frame = *push.frame;
  
  float4 world_position = mul(push.model, float4(vertex.position, 1.0));
  float4 view_position = mul(frame.view, world_position);
  
  vs_out output;

  output.uv = vertex.uv;
  output.position = mul(frame.projection, view_position);

  return output;
}

[shader("fragment")]
fs_out fragment_main(fs_in input) {
  
  frame_data frame = *push.frame;
  material_data material = frame.materials[push.material_index];
  
  float4 albedo = textures[material.albedo_index].Sample(samplers[push.sampler_index], input.uv);
  
  fs_out output;

  output.color = material.base_color_factor * albedo;

  return output;
}
