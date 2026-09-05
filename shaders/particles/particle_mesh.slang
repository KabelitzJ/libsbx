// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <common.slang>
#include <descriptors.slang>
#include <frame_data.slang>
#include <shading_policy.slang>

// Matches render::particle_mesh_instance (render_packet.hpp) byte for byte.
struct particle_mesh_instance {
  float4x4 model;
  float4 color;
}; // struct particle_mesh_instance

struct push_data {
  frame_data* frame_data;
  vertex* vertices;
  particle_mesh_instance* instances;
  uint instance_offset;
  uint material_index;
  uint sampler_index;
}; // struct push_data

[[vk::push_constant]] ConstantBuffer<push_data> push;

struct vertex_input {
  [[vk::location(0)]] uint vertex_id : SV_VertexID;
  [[vk::location(1)]] uint instance_id : SV_InstanceID;
}; // struct vertex_input

struct vertex_output {
  float2 uv : TEXCOORD;
  float4 color : COLOR;
  float4 sv_position : SV_Position;
}; // struct vertex_output

struct fragment_input {
  float2 uv : TEXCOORD;
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
  vertex v = push.vertices[input.vertex_id];
  frame_data frame_data = *push.frame_data;
  particle_mesh_instance instance = push.instances[push.instance_offset + input.instance_id];

  float4 world_position = mul(instance.model, float4(v.position, 1.0));

  vertex_output output;
  output.uv = v.uv;
  output.color = instance.color;
  output.sv_position = mul(frame_data.projection, mul(frame_data.view, world_position));

  return output;
}

// Unlit, matching the billboard particle shader's own style: sampled albedo * the material's own
// base_color_factor * the particle's instance color -- no lighting evaluation at all.
float4 sample_particle_color(fragment_input input) {
  frame_data frame_data = *push.frame_data;
  material_data material = frame_data.materials[push.material_index];

  float4 sampled = textures[material.albedo_index].Sample(samplers[push.sampler_index], input.uv);

  return sampled * material.base_color_factor * input.color;
}

[shader("fragment")]
additive_fragment_output fragment_main_additive(fragment_input input) {
  float4 color = sample_particle_color(input);

  additive_fragment_output output;
  output.color = float4(color.rgb * color.a, color.a);

  return output;
}

[shader("fragment")]
alpha_blend_fragment_output fragment_main_alpha_blend(fragment_input input) {
  float4 color = sample_particle_color(input);

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
