// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <common.slang>
#include <frame_data.slang>
#include <shading_policy.slang>

// Matches render::trail_vertex (render_packet.hpp) byte for byte. The ribbon's width extrusion and
// camera-facing orientation are already baked into each vertex's world-space position at extraction
// time (scene_renderer_module.cpp) -- this shader only transforms to clip space and shades.
struct trail_vertex {
  float3 position;
  float4 color;
}; // struct trail_vertex

struct push_data {
  frame_data* frame_data;
  trail_vertex* vertices;
  uint vertex_offset;
}; // struct push_data

[[vk::push_constant]] ConstantBuffer<push_data> push;

struct vertex_input {
  [[vk::location(0)]] uint vertex_id : SV_VertexID;
}; // struct vertex_input

struct vertex_output {
  float4 color : COLOR;
  float4 sv_position : SV_Position;
}; // struct vertex_output

struct fragment_input {
  float4 color : COLOR;
  float4 sv_position : SV_Position;
}; // struct fragment_input

// Single target, blended (one, one, add) straight onto the real scene color -- true additive, same
// reasoning as particle_billboard.slang's additive path.
struct additive_fragment_output {
  float4 color : SV_Target0;
}; // struct additive_fragment_output

struct alpha_blend_fragment_output : alpha_blend_target {
  float4 accumulator : SV_Target0;
  float revealage : SV_Target1;
}; // struct alpha_blend_fragment_output

[shader("vertex")]
vertex_output vertex_main(vertex_input input) {
  trail_vertex v = push.vertices[push.vertex_offset + input.vertex_id];
  frame_data frame_data = *push.frame_data;

  vertex_output output;
  output.color = v.color;
  output.sv_position = mul(frame_data.projection, mul(frame_data.view, float4(v.position, 1.0)));

  return output;
}

[shader("fragment")]
additive_fragment_output fragment_main_additive(fragment_input input) {
  additive_fragment_output output;
  output.color = float4(input.color.rgb * input.color.a, input.color.a);

  return output;
}

[shader("fragment")]
alpha_blend_fragment_output fragment_main_alpha_blend(fragment_input input) {
  shading_input shading_input;
  shading_input.lighting.color = input.color.rgb;
  shading_input.lighting.alpha = input.color.a;
  shading_input.depth = input.sv_position.z;

  alpha_blend_result result = compute_alpha_blend(shading_input);

  alpha_blend_fragment_output output;
  output.accumulator = result.accumulator;
  output.revealage = result.revealage;

  return output;
}
