// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <frame_data.slang>

// Mirrors sbx::render::debug_vertex (libsbx/render/debug/debug_draw.hpp).
struct debug_vertex {
  float4 position;
  float4 color;
}; // struct debug_vertex

struct push_data {
  frame_data* frame;
  debug_vertex* vertices;
}; // struct push_data

[[vk::push_constant]] ConstantBuffer<push_data> push;

struct vertex_input {
  uint vertex_id : SV_VertexID;
}; // struct vertex_input

struct vertex_output {
  [[vk::location(0)]] float4 color : TEXCOORD0;
  float4 sv_position : SV_Position;
}; // struct vertex_output

struct fragment_input {
  [[vk::location(0)]] float4 color : TEXCOORD0;
}; // struct fragment_input

struct fragment_output {
  float4 color : SV_Target0;
}; // struct fragment_output

[shader("vertex")]
vertex_output vertex_main(vertex_input input) {
  debug_vertex vertex = push.vertices[input.vertex_id];
  frame_data frame = *push.frame;

  vertex_output output;

  output.color = vertex.color;
  output.sv_position = mul(frame.projection, mul(frame.view, vertex.position));

  return output;
}

[shader("fragment")]
fragment_output fragment_main(fragment_input input) {
  fragment_output output;

  output.color = input.color;

  return output;
}
