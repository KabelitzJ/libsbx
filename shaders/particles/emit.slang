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

// One of these dispatches per active emitter instance that has particles to spawn this frame (see
// particle_simulate_pass.cpp), sized to that one emitter's particles_to_emit — never the whole
// pool. Every thread claims its own free slot via a single atomic decrement of dead_count, so
// concurrent spawns from different emitters (or different threads of the same emitter) can never
// collide on a slot — this is the fix for the old system's linear free-slot-scan race.
[shader("compute")]
[numthreads(64, 1, 1)]
void compute_main(cs_in input) {
  uint local_index = input.id.x;

  emitter_instance e = push.emitters[push.emitter_index];

  if (local_index >= e.particles_to_emit) {
    return;
  }

  // Atomic decrement via unsigned wraparound (there is no signed InterlockedAdd overload on a
  // uint field): `dead_index` is the value *before* this thread's decrement. If it was already 0,
  // the pool was exhausted and this decrement just wrapped it to 0xFFFFFFFF — undo it immediately
  // and drop the spawn rather than reading dead_list at index -1.
  uint dead_index;
  InterlockedAdd(push.counters[0].dead_count, 0xFFFFFFFFu, dead_index);

  if (dead_index == 0u) {
    InterlockedAdd(push.counters[0].dead_count, 1u);
    return;
  }

  uint slot = push.dead_list[dead_index - 1u];

  uint seed = pcg_hash(e.seed ^ push.emitter_index ^ local_index ^ asuint(push.time));

  particle p;
  p.position = e.position + sample_emission_shape(seed, e.shape, e.shape_extents);
  p.velocity = random_range3(seed, e.velocity_min, e.velocity_max);
  p.age = 0.0;
  p.lifetime = random_range(seed, e.lifetime_min, e.lifetime_max);
  p.size = random_range(seed, e.size_min, e.size_max);
  p.rotation = random_range(seed, 0.0, 6.28318530718);
  p.emitter_slot = push.emitter_index;
  p.seed = seed;
  p.reserved = float4(0.0, 0.0, 0.0, 0.0);

  push.particles[slot] = p;

  uint alive_index;
  InterlockedAdd(push.counters[0].alive_count[push.write_index], 1u, alive_index);

  if (alive_index < push.max_particles) {
    push.alive_list_write[alive_index] = slot;
  }
}
