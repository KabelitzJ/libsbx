// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <common.slang>
#include <descriptors.slang>
#include <frame_data.slang>
#include <shading_policy.slang>
#include <lighting.slang>

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
}; // struct fragment_input

struct opaque_fragment_output : color_target {
  float4 color : SV_Target0;
}; // struct fragment_output

struct alpha_blend_fragment_output : alpha_blend_target {
  float4 accumulator : SV_Target0;
  float revealage : SV_Target1;
}; // struct alpha_blend_fragment_output

struct push_data {
  frame_data* frame_data;
  vertex* vertices;
  float4x4* transforms;
  uint transform_offset;
  uint material_index;
  uint sampler_index;
  uint clamp_sampler_index;
}; // struct push_data

[[vk::push_constant]] ConstantBuffer<push_data> push;

typedef basic_color_shading_policy<opaque_fragment_output> opaque_shading_policy;
typedef basic_alpha_blend_shading_policy<alpha_blend_fragment_output> alpha_blend_shading_policy;

[shader("vertex")]
vertex_output vertex_main(vertex_input input) {
  vertex v = push.vertices[input.vertex_id];
  frame_data frame_data = *push.frame_data;

  float4x4 model = push.transforms[push.transform_offset + input.instance_id];

  float4 world = mul(model, float4(v.position, 1.0));
  float3x3 normal_matrix = (float3x3)model;

  vertex_output output;
  output.position = world.xyz;
  output.normal = normalize(mul(normal_matrix, v.normal));
  output.tangent = float4(normalize(mul(normal_matrix, v.tangent.xyz)), v.tangent.w);
  output.uv = v.uv;
  output.sv_position = mul(frame_data.projection, mul(frame_data.view, world));

  return output;
}

[shader("fragment")]
Policy::output fragment_main<Policy : shading_policy>(fragment_input input) {
  lighting_input lighting_input;
  lighting_input.position = input.position;
  lighting_input.normal = input.normal;
  lighting_input.uv = input.uv;
  lighting_input.tangent = input.tangent;
  lighting_input.sv_position = input.sv_position;
  lighting_input.frame_data = *push.frame_data;
  lighting_input.material_index = push.material_index;
  lighting_input.sampler_index = push.sampler_index;
  lighting_input.clamp_sampler_index = push.clamp_sampler_index;

  lighting_output lighting_output = evaluate_lighting<Policy::receives_shadows>(lighting_input);

  shading_input shading_input;
  shading_input.lighting = lighting_output;
  shading_input.depth = input.sv_position.z;

  return Policy::shade(shading_input);
}
