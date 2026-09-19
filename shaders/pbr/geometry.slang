// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <shading_policy.slang>

struct vertex_input {
  [[vk::location(0)]] uint vertex_id : SV_VertexID;
  [[vk::location(1)]] uint instance_id : SV_InstanceID;
}; // struct vertex_input

struct vertex_output {
  [[vk::location(0)]] float3 position : POSITION;
  [[vk::location(1)]] float3 normal : NORMAL;
  [[vk::location(2)]] float2 uv : TEXCOORD;
  [[vk::location(3)]] float4 tangent : TANGENT;
  [[vk::location(4)]] float4 color : COLOR;
  float4 sv_position : SV_Position;
}; // struct vertex_output

struct fragment_input {
  [[vk::location(0)]] float3 position : POSITION;
  [[vk::location(1)]] float3 normal : NORMAL;
  [[vk::location(2)]] float2 uv : TEXCOORD;
  [[vk::location(3)]] float4 tangent : TANGENT;
  [[vk::location(4)]] float4 color : COLOR;
  float4 sv_position : SV_Position;
  bool is_front_face : SV_IsFrontFace;
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
  transform_data* transforms;
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

  transform_data instance = push.transforms[push.transform_offset + input.instance_id];

  float4 world_position = mul(instance.model, float4(v.position, 1.0));

  vertex_output output;
  output.position = world_position.xyz;
  output.normal = normalize(mul(instance.normal, float4(v.normal, 0.0)).xyz);
  output.tangent = float4(normalize(mul(instance.normal, float4(v.tangent.xyz, 0.0)).xyz), v.tangent.w);
  output.uv = v.uv;
  output.color = v.color;
  output.sv_position = mul(frame_data.projection, mul(frame_data.view, world_position));

  return output;
}

lighting_input build_lighting_input(fragment_input input) {
  lighting_input result;
  result.position = input.position;
  result.normal = input.normal;
  result.uv = input.uv;
  result.tangent = input.tangent;
  result.color = input.color;
  result.sv_position = input.sv_position;
  result.is_front_face = input.is_front_face;
  result.frame_data = *push.frame_data;
  result.material_index = push.material_index;
  result.sampler_index = push.sampler_index;
  result.clamp_sampler_index = push.clamp_sampler_index;

  return result;
}

[shader("fragment")]
Policy::Output fragment_main_pbr<Policy : shading_policy>(fragment_input input) {
  lighting_input lighting_input = build_lighting_input(input);

  lighting_output lighting_output = evaluate_lighting<Policy::receives_shadows>(lighting_input);

  shading_input shading_input;
  shading_input.lighting = lighting_output;
  shading_input.depth = input.sv_position.z;

  return Policy::shade(shading_input);
}

[shader("fragment")]
Policy::Output fragment_main_unlit<Policy : shading_policy>(fragment_input input) {
  lighting_input lighting_input = build_lighting_input(input);

  lighting_output lighting_output = evaluate_unlit(lighting_input);

  shading_input shading_input;
  shading_input.lighting = lighting_output;
  shading_input.depth = input.sv_position.z;

  return Policy::shade(shading_input);
}

[shader("fragment")]
Policy::Output fragment_main_wireframe<Policy : shading_policy>(fragment_input input) {
  lighting_output lighting_output;
  lighting_output.color = float3(0.0, 0.0, 0.0);
  lighting_output.alpha = 1.0;

  shading_input shading_input;
  shading_input.lighting = lighting_output;
  shading_input.depth = input.sv_position.z;

  return Policy::shade(shading_input);
}
