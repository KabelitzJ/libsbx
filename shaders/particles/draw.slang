// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <frame_data.slang>
#include <descriptors.slang>
#include <shading_policy.slang>
#include <quad.slang>
#include <math.slang>

#include <particles/particle_data.slang>

struct vertex_input {
  [[vk::location(0)]] uint vertex_id : SV_VertexID;
  [[vk::location(1)]] uint instance_id : SV_InstanceID;
}; // struct vertex_input

struct vertex_output {
  [[vk::location(0)]] float2 uv : TEXCOORD;
  [[vk::location(1)]] float4 color : COLOR;
  [[vk::location(2)]] nointerpolation uint texture_index : TEXTURE_INDEX;
  float4 sv_position : SV_Position;
}; // struct vertex_output

struct fragment_input {
  [[vk::location(0)]] float2 uv : TEXCOORD;
  [[vk::location(1)]] float4 color : COLOR;
  [[vk::location(2)]] nointerpolation uint texture_index : TEXTURE_INDEX;
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

struct push_data {
  frame_data* frame;
  particle* particles;
  uint* alive_list;
  emitter_instance* emitters;
  uint sampler_index;
}; // struct push_data

[[vk::push_constant]] ConstantBuffer<push_data> push;

[shader("vertex")]
vertex_output vertex_main(vertex_input input) {
  frame_data frame = *push.frame;

  uint slot = push.alive_list[input.instance_id];
  particle particle = push.particles[slot];
  emitter_instance emitter = push.emitters[particle.emitter_slot];

  float t = clamp(particle.age / max(particle.lifetime, 0.0001), 0.0, 1.0);
  float4 color = lerp(emitter.start_color, emitter.end_color, t);

  int index = quad::indices[input.vertex_id];
  float2 local_position = quad::vertices[index];

  float3x3 inverse_view = transpose((float3x3)frame.view);
  float3 camera_right = normalize(mul(inverse_view, float3(1.0, 0.0, 0.0)));
  float3 camera_up = normalize(mul(inverse_view, float3(0.0, 1.0, 0.0)));

  float2 rotated = rotate(local_position, particle.rotation);
  float3 offset = camera_right * rotated.x * particle.size + camera_up * rotated.y * particle.size;
  float3 world_position = particle.position + offset;

  vertex_output output;

  output.sv_position = mul(frame.projection, mul(frame.view, float4(world_position, 1.0)));
  output.uv = local_position * 0.5 + 0.5;
  output.color = color;
  output.texture_index = emitter.texture_index;

  return output;
}

float4 sample_particle_color(fragment_input input) {
  float4 color = input.color;

  if (input.texture_index != particle_texture_index_none) {
    SamplerState sampler = samplers[push.sampler_index];
    color *= textures[NonUniformResourceIndex(input.texture_index)].Sample(sampler, input.uv);
  } else {
    float distance_from_center = length(input.uv * 2.0 - 1.0);
    float mask = saturate(1.0 - smoothstep(0.7, 1.0, distance_from_center));
    color.a *= mask;
  }

  return color;
}

// Premultiplied on purpose, same as particle_billboard.slang's fragment_main_additive: alpha here
// only scales the contribution's brightness, it must never end up subtracting from the destination
// (that's what an additive (ONE, ONE) blend needs to actually look additive at alpha < 1).
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

  shading_input shading_in;
  shading_in.lighting.color = color.rgb;
  shading_in.lighting.alpha = color.a;
  shading_in.depth = input.sv_position.z;

  alpha_blend_result result = compute_alpha_blend(shading_in);

  alpha_blend_fragment_output output;
  output.accumulator = result.accumulator;
  output.revealage = result.revealage;

  return output;
}
