// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef GEOMETRY_COMMON_SLANG_
#define GEOMETRY_COMMON_SLANG_

#include <common.slang>
#include <descriptors.slang>
#include <frame_data.slang>
#include <lighting.slang>
#include <shading_policy.slang>

// Everything a shader-graph-generated file needs besides its own vertex/fragment graph logic:
// shared structs, push constants, and the fragment_input -> lighting_input adapter -- identical to
// geometry.slang's own copies (kept separate/self-contained there by choice) so a generated file
// only needs `#include <geometry_common.slang>` plus its own graph_vertex_*/fragment_main.
//
// vertex_main itself is NOT shared here (unlike the rest of this file) -- every generated graph
// file emits its own (shader_graph_codegen.cpp), since it needs to route through that graph's own
// Vertex block (Position/Normal/Tangent, defaulting to pass-through). Keeping it here would collide
// with the generated file's own definition.

struct vertex_input {
  [[vk::location(0)]] uint vertex_id : SV_VertexID;
  [[vk::location(1)]] uint instance_id : SV_InstanceID;
}; // struct vertex_input

struct vertex_output {
  [[vk::location(0)]] float3 position : POSITION;
  [[vk::location(1)]] float3 normal : NORMAL;
  [[vk::location(2)]] float2 uv : TEXCOORD;
  [[vk::location(3)]] float4 tangent : TANGENT;
  float4 sv_position : SV_Position;
}; // struct vertex_output

struct fragment_input {
  [[vk::location(0)]] float3 position : POSITION;
  [[vk::location(1)]] float3 normal : NORMAL;
  [[vk::location(2)]] float2 uv : TEXCOORD;
  [[vk::location(3)]] float4 tangent : TANGENT;
  float4 sv_position : SV_Position;
  bool is_front_face : SV_IsFrontFace;
}; // struct fragment_input

struct opaque_fragment_output : color_target {
  float4 color : SV_Target0;
}; // struct opaque_fragment_output

struct alpha_blend_fragment_output : alpha_blend_target {
  float4 accumulator : SV_Target0;
  float revealage : SV_Target1;
}; // struct alpha_blend_fragment_output

typedef basic_color_shading_policy<opaque_fragment_output> opaque_shading_policy;
typedef basic_alpha_blend_shading_policy<alpha_blend_fragment_output> alpha_blend_shading_policy;

struct push_data {
  frame_data* frame_data;
  vertex* vertices;
  transform_data* transforms;
  uint transform_offset;
  uint material_index;
  uint sampler_index;
  uint clamp_sampler_index;

  // 0xFFFFFFFF outside shadow_pass (render_pass.hpp's push_constants::cascade_index default,
  // matching shaders/passes/shadow.slang's own field) -- a generated file's depth_vertex_main
  // reads this to pick between camera-space (depth_pre_pass) and this cascade's light-space
  // (shadow_pass) transform, since both passes share that one entry point. vertex_main/
  // fragment_main<Policy> never reference it (always camera-space, always this default) --
  // shared here purely because it must occupy the same push-constant byte offset the engine's own
  // push_constants struct (render_pass.hpp) always writes it at, regardless of which entry point
  // in this file ends up bound.
  uint cascade_index;
}; // struct push_data

[[vk::push_constant]] ConstantBuffer<push_data> push;

lighting_input build_lighting_input(fragment_input input) {
  lighting_input result;
  result.position = input.position;
  result.normal = input.normal;
  result.uv = input.uv;
  result.tangent = input.tangent;
  result.sv_position = input.sv_position;
  result.is_front_face = input.is_front_face;
  result.frame_data = *push.frame_data;
  result.material_index = push.material_index;
  result.sampler_index = push.sampler_index;
  result.clamp_sampler_index = push.clamp_sampler_index;

  return result;
}

#endif // GEOMETRY_COMMON_SLANG_
