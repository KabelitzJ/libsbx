// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

// Selected geometry as a white silhouette for selection_outline_pass (Hazel's SelectedGeometry).

#include <frame_data.slang>

struct vertex_input {
  uint vertex_id : SV_VertexID;
  uint instance_id : SV_InstanceID;
}; // struct vertex_input

struct vertex_output {
  float4 sv_position : SV_Position;
}; // struct vertex_output

struct fragment_output {
  float4 color : SV_Target0;
}; // struct fragment_output

// A prefix of render_pass.hpp's push_constants.
struct push_data {
  frame_data* frame;
  vertex* vertices;
  transform_data* transforms;
  uint transform_offset;
}; // struct push_data

[[vk::push_constant]] ConstantBuffer<push_data> push;

[shader("vertex")]
vertex_output vertex_main(vertex_input input) {
  vertex vertex = push.vertices[input.vertex_id];
  frame_data frame = push.frame[0];

  float4x4 model = push.transforms[push.transform_offset + input.instance_id].model;
  float4 world = mul(model, float4(vertex.position, 1.0));

  vertex_output output;

  output.sv_position = mul(frame.projection, mul(frame.view, world));

  return output;
}

[shader("fragment")]
fragment_output fragment_main() {
  fragment_output output;

  output.color = float4(1.0);

  return output;
}
