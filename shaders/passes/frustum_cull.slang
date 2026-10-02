// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <common.slang>
#include <frame_data.slang>
#include <passes/frustum_cull_common.slang>

struct push_data {
  frame_data* frame_address;
  transform_data* source_transforms;
  transform_data* destination_transforms;
  draw_indexed_indirect_command* indirect_args;
  float4 local_bounds_min;
  float4 local_bounds_max;
  uint command_index;
  uint transform_offset;
  uint instance_count;
  uint cascade_index; // 0xFFFFFFFF = camera frustum, otherwise that shadow cascade's light frustum
}; // struct push_data

[[vk::push_constant]]
ConstantBuffer<push_data> push;

struct compute_input {
  uint3 id : SV_DispatchThreadID;
}; // struct compute_input

[shader("compute")]
[numthreads(64, 1, 1)]
void compute_main(compute_input input) {
  uint instance_id = input.id.x;

  if (instance_id >= push.instance_count) {
    return;
  }

  transform_data transform_data = push.source_transforms[push.transform_offset + instance_id];

  frame_data frame = push.frame_address[0];

  if (!instance_visible(frame, push.cascade_index, transform_data.model, push.local_bounds_min.xyz, push.local_bounds_max.xyz)) {
    return;
  }

  uint slot;
  InterlockedAdd(push.indirect_args[push.command_index].instance_count, 1u, slot);

  store_transform(push.destination_transforms + push.transform_offset + slot, transform_data);
}
