// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <frame_data.slang>

struct vertex_input {
  uint vertex_id : SV_VertexID;
  uint instance_id : SV_InstanceID;
}; // struct vertex_input

struct vertex_output {
  float4 sv_position : SV_Position;
}; // struct vertex_output

struct push_data {
  frame_data* frame;
  vertex* vertices;
  float4x4* transforms;
  uint transform_offset;
}; // struct push_data

[[vk::push_constant]] ConstantBuffer<push_data> push;

[shader("vertex")]
vertex_output vertex_main(vertex_input input) {
  vertex vertex = push.vertices[input.vertex_id];
  frame_data frame = push.frame[0];

  float4x4 model = push.transforms[push.transform_offset + input.instance_id];
  float4 world = mul(model, float4(vertex.position, 1.0));

  vertex_output output;

  output.sv_position = mul(frame.projection, mul(frame.view, world));

  return output;
}

[shader("fragment")]
void fragment_main() {

}
