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
  float4 lod_errors; // levels 1 to lod_count - 1
  uint transform_offset;
  uint instance_count; // also the stride between levels' transform blocks
  uint cascade_index; // 0xFFFFFFFF = camera frustum, otherwise that shadow view's light frustum
  uint lod_count;
  uint occlusion_offset; // first index of this command's instances in frame.occlusion_visibility
  uint occlusion_mode;   // occlusion_mode_* (frustum_cull_common.slang)
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

  if (push.occlusion_mode == occlusion_mode_early && frame.occlusion_visibility[push.occlusion_offset + instance_id] == 0u) {
    return;
  }

  if (push.occlusion_mode == occlusion_mode_late) {
    uint index = push.occlusion_offset + instance_id;
    bool was_visible = frame.occlusion_visibility[index] != 0u;
    bool is_visible = !instance_occluded(frame, transform_data.model, push.local_bounds_min.xyz, push.local_bounds_max.xyz);

    frame.occlusion_visibility[index] = is_visible ? 1u : 0u;

    if (!is_visible) {
      InterlockedAdd(frame.occlusion_culled_counter[0], 1u);
    }

    // Hidden, or already drawn by the early cull: nothing more to draw.
    if (!is_visible || was_visible) {
      return;
    }
  }

  lod_choice lod = choose_lod(frame, push.cascade_index, transform_data.model, push.local_bounds_min.xyz, push.local_bounds_max.xyz, push.lod_count, push.lod_errors);

  // While crossfading, the instance goes to both levels: the current one fading out (fade > 1), the next fading in.
  transform_data.normal[3][3] = lod.next_fade > 0.0 ? 1.0 + lod.next_fade : 1.0;

  uint slot;
  InterlockedAdd(push.indirect_args[lod.level].instance_count, 1u, slot);
  store_transform(push.destination_transforms + lod.level * push.instance_count + slot, transform_data);

  if (lod.next_fade > 0.0) {
    transform_data.normal[3][3] = lod.next_fade;

    InterlockedAdd(push.indirect_args[lod.level + 1u].instance_count, 1u, slot);
    store_transform(push.destination_transforms + (lod.level + 1u) * push.instance_count + slot, transform_data);
  }
}
