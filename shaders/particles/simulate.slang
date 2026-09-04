// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <particles/particle_data.slang>

static const uint threads_per_group = 64u;

struct push_data {
  particle* particles;
  uint* dead_list;
  uint* alive_list_read;
  uint* alive_list_write;
  particle_counters* counters;
  emitter_instance* emitters;
  float delta_time;
  uint read_index;
  uint write_index;
  uint max_particles;
}; // struct push_data

[[vk::push_constant]] ConstantBuffer<push_data> push;

struct compute_input {
  uint3 id : SV_DispatchThreadID;
}; // struct compute_input

[shader("compute")]
[numthreads(64, 1, 1)]
void compute_main(compute_input input) {
  uint index = input.id.x;

  if (index >= push.counters->alive_count[push.read_index]) {
    return;
  }

  uint slot = push.alive_list_read[index];
  particle particle = push.particles[slot];

  particle.age += push.delta_time;

  if (particle.age >= particle.lifetime) {
    uint dead_index;
    InterlockedAdd(push.counters->dead_count, 1u, dead_index);

    if (dead_index < push.max_particles) {
      push.dead_list[dead_index] = slot;
    }

    return;
  }

  emitter_instance emitter = push.emitters[particle.emitter_slot];

  float3 old_velocity = particle.velocity;

  particle.velocity += float3(0.0, -1.0, 0.0) * emitter.gravity * push.delta_time;
  particle.velocity *= exp(-emitter.drag * push.delta_time);
  particle.position += 0.5 * (old_velocity + particle.velocity) * push.delta_time;

  push.particles[slot] = particle;

  uint alive_index;
  InterlockedAdd(push.counters->alive_count[push.write_index], 1u, alive_index);

  if (alive_index < push.max_particles) {
    push.alive_list_write[alive_index] = slot;
  }
}
