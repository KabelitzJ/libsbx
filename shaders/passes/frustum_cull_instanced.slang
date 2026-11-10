// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <common.slang>
#include <frame_data.slang>
#include <passes/frustum_cull_common.slang>

// frustum_cull.slang for an instanced_mesh_renderer: instances come compact from the renderer's own
// buffer (scenes::instance_data: a 3x4 transform relative to the node, color, custom_data); each
// visible one is placed by the node's transform (node_transforms[node_index].model) and written
// out as a full transform_data, normal matrix included -- so only visible instances are expanded.

struct instance_data {
  float4 rows[3];
  float4 color;
  float4 custom_data;
}; // struct instance_data

struct push_data {
  frame_data* frame_address;
  transform_data* node_transforms;
  instance_data* instances;
  transform_data* destination_transforms;
  draw_indexed_indirect_command* indirect_args;
  uint2 padding;  // keeps the float4s at offset 48, where C++'s 4-byte-aligned math::vector4 puts them too
  float4 local_bounds_min;
  float4 local_bounds_max;
  float4 lod_errors; // levels 1 to lod_count - 1
  uint node_index;
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

  instance_data instance = push.instances[instance_id];
  float4x4 local = float4x4(instance.rows[0], instance.rows[1], instance.rows[2], float4(0.0, 0.0, 0.0, 1.0));
  float4x4 model = mul(push.node_transforms[push.node_index].model, local);

  frame_data frame = push.frame_address[0];

  if (!instance_visible(frame, push.cascade_index, model, push.local_bounds_min.xyz, push.local_bounds_max.xyz)) {
    return;
  }

  if (push.occlusion_mode == occlusion_mode_early && frame.occlusion_visibility[push.occlusion_offset + instance_id] == 0u) {
    return;
  }

  if (push.occlusion_mode == occlusion_mode_late) {
    uint index = push.occlusion_offset + instance_id;
    bool was_visible = frame.occlusion_visibility[index] != 0u;
    bool is_visible = !instance_occluded(frame, model, push.local_bounds_min.xyz, push.local_bounds_max.xyz);

    frame.occlusion_visibility[index] = is_visible ? 1u : 0u;

    if (!is_visible) {
      InterlockedAdd(frame.occlusion_culled_counter[0], 1u);
    }

    // Hidden, or already drawn by the early cull: nothing more to draw.
    if (!is_visible || was_visible) {
      return;
    }
  }

  // Normal matrix: the inverse transpose of the linear part, as its cofactors over the determinant
  // (rows of a matrix applied as mul(m, v)).
  float3 r0 = model[0].xyz;
  float3 r1 = model[1].xyz;
  float3 r2 = model[2].xyz;
  float determinant = dot(r0, cross(r1, r2));
  float inverse = abs(determinant) > 1e-20 ? 1.0 / determinant : 0.0;

  transform_data result;
  result.model = model;
  result.normal = float4x4(float4(cross(r1, r2) * inverse, 0.0), float4(cross(r2, r0) * inverse, 0.0), float4(cross(r0, r1) * inverse, 0.0), float4(0.0, 0.0, 0.0, 1.0));
  result.color = instance.color;
  result.custom_data = instance.custom_data;

  lod_choice lod = choose_lod(frame, push.cascade_index, model, push.local_bounds_min.xyz, push.local_bounds_max.xyz, push.lod_count, push.lod_errors);

  // While crossfading, the instance goes to both levels: the current one fading out (fade > 1), the next fading in.
  result.normal[3][3] = lod.next_fade > 0.0 ? 1.0 + lod.next_fade : 1.0;

  uint slot;
  InterlockedAdd(push.indirect_args[lod.level].instance_count, 1u, slot);
  store_transform(push.destination_transforms + lod.level * push.instance_count + slot, result);

  if (lod.next_fade > 0.0) {
    result.normal[3][3] = lod.next_fade;

    InterlockedAdd(push.indirect_args[lod.level + 1u].instance_count, 1u, slot);
    store_transform(push.destination_transforms + (lod.level + 1u) * push.instance_count + slot, result);
  }
}
