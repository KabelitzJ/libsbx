// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <frame_data.slang>
#include <descriptors.slang>

#include <particles/particle_data.slang>

struct vs_in {
  [[vk::location(0)]] uint vertex_id : SV_VertexID;
  [[vk::location(1)]] uint instance_id : SV_InstanceID;
}; // struct vs_in

struct vs_out {
  [[vk::location(0)]] float2 uv : TEXCOORD;
  [[vk::location(1)]] float4 color : COLOR;
  [[vk::location(2)]] nointerpolation uint texture_index : TEXTURE_INDEX;
  float4 sv_position : SV_Position;
}; // struct vs_out

struct fs_in {
  [[vk::location(0)]] float2 uv : TEXCOORD;
  [[vk::location(1)]] float4 color : COLOR;
  [[vk::location(2)]] nointerpolation uint texture_index : TEXTURE_INDEX;
  float4 sv_position : SV_Position;
}; // struct fs_in

struct fs_out {
  float4 color : SV_Target0;
}; // struct fs_out

struct transparent_fs_out {
  float4 accumulator : SV_Target0;
  float revealage : SV_Target1;
}; // struct transparent_fs_out

interface shading_policy {
  associatedtype Output;
  static Output shade(float4 color, fs_in input);
}; // interface shading_policy

struct additive_shading_policy : shading_policy {

  typedef fs_out Output;

  static fs_out shade(float4 color, fs_in input) {
    fs_out output;

    output.color = color;

    return output;
  }

}; // struct additive_shading_policy

struct alpha_blend_shading_policy : shading_policy {

  typedef transparent_fs_out Output;

  static transparent_fs_out shade(float4 color, fs_in input) {
    float alpha = clamp(color.a, 0.01, 0.99);
    float weight_alpha = pow(min(1.0, alpha * 10.0) + 0.01, 3.0);
    float weight_depth = pow(1.0 - input.sv_position.z * 0.9, 3.0);
    float weight = clamp(weight_alpha * 1e8 * weight_depth, 1e-2, 3e3);

    transparent_fs_out output;

    output.accumulator = float4(color.rgb * alpha, alpha) * weight;
    output.revealage = alpha;

    return output;
  }

}; // struct alpha_blend_shading_policy

struct push_data {
  frame_data* frame;
  particle* particles;
  uint* alive_list;
  emitter_instance* emitters;
  uint sampler_index;
}; // struct push_data

[[vk::push_constant]] ConstantBuffer<push_data> push;

static const float2 quad_vertices[4] = {
  float2(-1.0, -1.0),
  float2( 1.0, -1.0),
  float2( 1.0,  1.0),
  float2(-1.0,  1.0)
};

static const int quad_indices[6] = { 0, 1, 2, 2, 3, 0 };

float2 rotate(float2 position, float rotation) {
  float c = cos(rotation);
  float s = sin(rotation);

  return float2(
    position.x * c - position.y * s,
    position.x * s + position.y * c
  );
}

[shader("vertex")]
vs_out vertex_main(vs_in input) {
  frame_data frame = *push.frame;

  uint slot = push.alive_list[input.instance_id];
  particle particle = push.particles[slot];
  emitter_instance emitter = push.emitters[particle.emitter_slot];

  float t = clamp(particle.age / max(particle.lifetime, 0.0001), 0.0, 1.0);
  float4 color = lerp(emitter.start_color, emitter.end_color, t);

  int index = quad_indices[input.vertex_id];
  float2 local_position = quad_vertices[index];

  float3x3 inverse_view = transpose((float3x3)frame.view);
  float3 camera_right = normalize(mul(inverse_view, float3(1.0, 0.0, 0.0)));
  float3 camera_up = normalize(mul(inverse_view, float3(0.0, 1.0, 0.0)));

  float2 rotated = rotate(local_position, particle.rotation);
  float3 offset = camera_right * rotated.x * particle.size + camera_up * rotated.y * particle.size;
  float3 world_position = particle.position + offset;

  vs_out output;
  output.sv_position = mul(frame.projection, mul(frame.view, float4(world_position, 1.0)));
  output.uv = local_position * 0.5 + 0.5;
  output.color = color;
  output.texture_index = emitter.texture_index;

  return output;
}

[shader("fragment")]
Policy::Output fragment_main<Policy : shading_policy>(fs_in input) {
  float4 color = input.color;

  if (input.texture_index != particle_texture_index_none) {
    SamplerState sampler = samplers[push.sampler_index];
    color *= textures[NonUniformResourceIndex(input.texture_index)].Sample(sampler, input.uv);
  } else {
    float distance_from_center = length(input.uv * 2.0 - 1.0);
    float mask = saturate(1.0 - smoothstep(0.7, 1.0, distance_from_center));
    color.a *= mask;
  }

  return Policy::shade(color, input);
}
