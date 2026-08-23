// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <particles/particle_data.slang>

struct push_data {
  particle_counters* counters;
  uint* draw_args; // VkDrawIndirectCommand: {vertex_count, instance_count, first_vertex, first_instance}
  uint write_index;
}; // struct push_data

[[vk::push_constant]] ConstantBuffer<push_data> push;

// Single thread: sizes this frame's draw_indirect to the alive list simulate.slang/emit.slang just
// finished building (push.write_index) — vertex.slang procedurally builds a 6-vertex quad per
// instance, so vertex_count is fixed.
[shader("compute")]
[numthreads(1, 1, 1)]
void compute_main() {
  push.draw_args[0] = 6u;
  push.draw_args[1] = push.counters[0].alive_count[push.write_index];
  push.draw_args[2] = 0u;
  push.draw_args[3] = 0u;
}
