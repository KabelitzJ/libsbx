// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <common.slang>

#include <particles/particle_data.slang>

struct push_data {
  particle_counters* counters;
  draw_indirect_command* command;
  uint write_index;
}; // struct push_data

[[vk::push_constant]] ConstantBuffer<push_data> push;

[shader("compute")]
[numthreads(1, 1, 1)]
void compute_main() {
  push.command->vertex_count = 6u;
  push.command->instance_count = push.counters->alive_count[push.write_index];
  push.command->first_vertex = 0u;
  push.command->first_instance = 0u;
}
