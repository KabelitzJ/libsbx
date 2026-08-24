// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <particles/particle_data.slang>

struct push_data {
  particle* particles;
  uint* dead_list;
  uint* alive_list_write;
  particle_counters* counters;
  emitter_instance* emitters;
  uint emitter_index;
  uint write_index;
  uint max_particles;
  float time;
}; // struct push_data

[[vk::push_constant]] ConstantBuffer<push_data> push;

struct cs_in {
  uint3 id : SV_DispatchThreadID;
}; // struct cs_in

[shader("compute")]
[numthreads(64, 1, 1)]
void compute_main(cs_in input) {
  uint local_index = input.id.x;

  emitter_instance emitter = push.emitters[push.emitter_index];

  if (local_index >= emitter.particles_to_emit) {
    return;
  }

  uint dead_index;
  InterlockedAdd(push.counters->dead_count, 0xFFFFFFFFu, dead_index);

  if (dead_index == 0u) {
    InterlockedAdd(push.counters->dead_count, 1u);
    return;
  }

  uint slot = push.dead_list[dead_index - 1u];

  uint seed = pcg_hash(emitter.seed ^ push.emitter_index ^ local_index ^ asuint(push.time));

  particle particle;
  particle.position = emitter.position + sample_emission_shape(seed, emitter.shape, emitter.shape_extents);
  particle.velocity = random_range3(seed, emitter.velocity_min, emitter.velocity_max);
  particle.age = 0.0;
  particle.lifetime = random_range(seed, emitter.lifetime_min, emitter.lifetime_max);
  particle.size = random_range(seed, emitter.size_min, emitter.size_max);
  particle.rotation = random_range(seed, 0.0, 6.28318530718);
  particle.emitter_slot = push.emitter_index;
  particle.seed = seed;
  particle.reserved = float4(0.0, 0.0, 0.0, 0.0);

  push.particles[slot] = particle;

  uint alive_index;
  InterlockedAdd(push.counters->alive_count[push.write_index], 1u, alive_index);

  if (alive_index < push.max_particles) {
    push.alive_list_write[alive_index] = slot;
  }
}
