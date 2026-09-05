// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <frame_data.slang>
#include <skinning/skin_data.slang>

// One dispatch skins one skinned-mesh instance's whole vertex range (see
// libsbx/render/render_packet.hpp's skin_dispatch doc comment) into scene_renderer_module's shared
// scratch vertex buffer, read afterward by depth_pre_pass/shadow_pass/opaque_pass exactly like a
// static mesh's own vertex buffer -- those three shaders need no awareness this pass exists.
struct push_data {
  vertex* source_vertices;
  skin_vertex* source_skin_vertices;
  float4x4* palette;
  vertex* output_vertices;
  uint joint_offset;
  uint vertex_count;
}; // struct push_data

[[vk::push_constant]] ConstantBuffer<push_data> push;

struct compute_input {
  uint3 id : SV_DispatchThreadID;
}; // struct compute_input

[shader("compute")]
[numthreads(64, 1, 1)]
void compute_main(compute_input input) {
  uint index = input.id.x;

  if (index >= push.vertex_count) {
    return;
  }

  vertex source = push.source_vertices[index];
  skin_vertex skin = push.source_skin_vertices[index];

  float4x4 skin_matrix =
      push.palette[push.joint_offset + skin.joint_indices.x] * skin.weights.x +
      push.palette[push.joint_offset + skin.joint_indices.y] * skin.weights.y +
      push.palette[push.joint_offset + skin.joint_indices.z] * skin.weights.z +
      push.palette[push.joint_offset + skin.joint_indices.w] * skin.weights.w;

  vertex result;
  result.position = mul(skin_matrix, float4(source.position, 1.0)).xyz;
  result.normal = normalize(mul(skin_matrix, float4(source.normal, 0.0)).xyz);
  result.tangent = float4(normalize(mul(skin_matrix, float4(source.tangent.xyz, 0.0)).xyz), source.tangent.w);
  result.uv = source.uv;

  push.output_vertices[index] = result;
}
