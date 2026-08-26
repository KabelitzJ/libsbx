// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <common.slang>

#include <particles/particle_data.slang>

static const uint simulate_threads_per_group = 64u;

struct push_data {
  particle_counters* counters;
  dispatch_indirect_command* dispatch_args;
  uint read_index;
  uint write_index;
}; // struct push_data

[[vk::push_constant]] ConstantBuffer<push_data> push;

[shader("compute")]
[numthreads(1, 1, 1)]
void compute_main() {
  uint alive_count = push.counters->alive_count[push.read_index];

  push.dispatch_args->group_count_x = (alive_count + simulate_threads_per_group - 1u) / simulate_threads_per_group;
  push.dispatch_args->group_count_y = 1u;
  push.dispatch_args->group_count_z = 1u;

  push.counters->alive_count[push.write_index] = 0u;
}
