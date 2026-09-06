// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

// Mirrors sbx::canvas::canvas_vertex (libsbx/canvas/canvas_draw_list.hpp). Unlike debug_draw.slang,
// there's no frame_data/view/projection multiply here -- canvas_draw_list::add_rect already
// resolves every vertex straight to NDC, since a screen-space-overlay element's position doesn't
// depend on any camera.
struct canvas_vertex {
  float4 position;
  float4 color;
}; // struct canvas_vertex

struct push_data {
  canvas_vertex* vertices;
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
  canvas_vertex vertex = push.vertices[input.vertex_id];

  vertex_output output;

  output.color = vertex.color;
  output.sv_position = vertex.position;

  return output;
}

[shader("fragment")]
fragment_output fragment_main(fragment_input input) {
  fragment_output output;

  output.color = input.color;

  return output;
}
