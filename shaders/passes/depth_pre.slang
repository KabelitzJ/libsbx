// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <frame_data.slang>

struct vertex_input {
  uint vertex_id : SV_VertexID;
  uint instance_id : SV_InstanceID;
}; // struct vertex_input

struct vertex_output {
  float2 uv : TEXCOORD;
  float4 sv_position : SV_Position;
}; // struct vertex_output

struct fragment_input {
  float2 uv : TEXCOORD;
  float4 sv_position : SV_Position;
}; // struct fragment_input

struct push_data {
  frame_data* frame;
  vertex* vertices;
  transform_data* transforms;
  uint transform_offset;
  uint material_index;
  uint sampler_index;
}; // struct push_data

[[vk::push_constant]] ConstantBuffer<push_data> push;

[shader("vertex")]
vertex_output vertex_main(vertex_input input) {
  vertex vertex = push.vertices[input.vertex_id];
  frame_data frame = push.frame[0];

  float4x4 model = push.transforms[push.transform_offset + input.instance_id].model;
  float4 world = mul(model, float4(vertex.position, 1.0));

  vertex_output output;

  output.uv = vertex.uv;
  output.sv_position = mul(frame.projection, mul(frame.view, world));

  return output;
}

[shader("fragment")]
void fragment_main(fragment_input input) {
  frame_data frame = push.frame[0];
  material_data material = frame.materials[push.material_index];

  apply_alpha_cutout(material, input.uv, push.sampler_index);
}
