// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <particle_data.slang>

// Group size simulate.slang dispatches with — must match its own [numthreads].
static const uint simulate_threads_per_group = 64u;

struct push_data {
  particle_counters* counters;
  uint* dispatch_args; // VkDispatchIndirectCommand: {group_count_x, group_count_y, group_count_z}
  uint read_index;
  uint write_index;
}; // struct push_data

[[vk::push_constant]] ConstantBuffer<push_data> push;

// Single thread: sizes this frame's indirect simulate dispatch to last frame's alive count (not
// max_particles — the whole point of the indirect dispatch), and clears the alive-count slot this
// frame's simulate/emit are about to append into. Must run before both.
[shader("compute")]
[numthreads(1, 1, 1)]
void compute_main() {
  uint alive_count = push.counters[0].alive_count[push.read_index];

  push.dispatch_args[0] = (alive_count + simulate_threads_per_group - 1u) / simulate_threads_per_group;
  push.dispatch_args[1] = 1u;
  push.dispatch_args[2] = 1u;

  push.counters[0].alive_count[push.write_index] = 0u;
}
