// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <common.slang>
#include <descriptors.slang>
#include <frame_data.slang>
#include <quad.slang>
#include <shading_policy.slang>

struct particle_billboard_instance {
  float3 position;
  float size;
  float4 color;
  float rotation;
  uint texture_index;
  float2 padding;
}; // struct particle_billboard_instance

struct push_data {
  frame_data* frame_data;
  particle_billboard_instance* instances;
  uint instance_offset;
  uint sampler_index;
  uint _padding0;
  uint _padding1;
  float4 camera_right;
  float4 camera_up;
}; // struct push_data

[[vk::push_constant]] ConstantBuffer<push_data> push;

struct vertex_input {
  [[vk::location(0)]] uint vertex_id : SV_VertexID;
  [[vk::location(1)]] uint instance_id : SV_InstanceID;
}; // struct vertex_input

struct vertex_output {
  float2 uv : TEXCOORD;
  float4 color : COLOR;
  nointerpolation uint texture_index : TEXTURE_INDEX;
  float4 sv_position : SV_Position;
}; // struct vertex_output

struct fragment_input {
  float2 uv : TEXCOORD;
  float4 color : COLOR;
  nointerpolation uint texture_index : TEXTURE_INDEX;
  float4 sv_position : SV_Position;
}; // struct fragment_input

// Single target, blended (one, one, add) straight onto the real scene color -- true additive, not
// routed through the weighted-OIT accumulator/revealage pair alpha_blend uses below (see
// particle_pass's doc comment for why that combination would be wrong for a glow effect).
struct additive_fragment_output {
  float4 color : SV_Target0;
}; // struct additive_fragment_output

struct alpha_blend_fragment_output : alpha_blend_target {
  float4 accumulator : SV_Target0;
  float revealage : SV_Target1;
}; // struct alpha_blend_fragment_output

[shader("vertex")]
vertex_output vertex_main(vertex_input input) {
  particle_billboard_instance instance = push.instances[push.instance_offset + input.instance_id];
  frame_data frame_data = *push.frame_data;

  float2 corner = quad::vertices[quad::indices[input.vertex_id]];

  float cos_r = cos(instance.rotation);
  float sin_r = sin(instance.rotation);
  float2 rotated_corner = float2(corner.x * cos_r - corner.y * sin_r, corner.x * sin_r + corner.y * cos_r);

  float3 world_position = instance.position + (push.camera_right.xyz * rotated_corner.x + push.camera_up.xyz * rotated_corner.y) * (instance.size * 0.5);

  vertex_output output;
  output.uv = corner * 0.5 + 0.5;
  output.color = instance.color;
  output.texture_index = instance.texture_index;
  output.sv_position = mul(frame_data.projection, mul(frame_data.view, float4(world_position, 1.0)));

  return output;
}

[shader("fragment")]
additive_fragment_output fragment_main_additive(fragment_input input) {
  float4 sampled = textures[input.texture_index].Sample(samplers[push.sampler_index], input.uv);
  float4 color = sampled * input.color;

  additive_fragment_output output;
  output.color = float4(color.rgb * color.a, color.a);

  return output;
}

[shader("fragment")]
alpha_blend_fragment_output fragment_main_alpha_blend(fragment_input input) {
  float4 sampled = textures[input.texture_index].Sample(samplers[push.sampler_index], input.uv);
  float4 color = sampled * input.color;

  shading_input shading_input;
  shading_input.lighting.color = color.rgb;
  shading_input.lighting.alpha = color.a;
  shading_input.depth = input.sv_position.z;

  alpha_blend_result result = compute_alpha_blend(shading_input);

  alpha_blend_fragment_output output;
  output.accumulator = result.accumulator;
  output.revealage = result.revealage;

  return output;
}
