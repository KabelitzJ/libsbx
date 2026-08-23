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
  float dt;
  uint read_index;
  uint write_index;
  uint max_particles;
}; // struct push_data

[[vk::push_constant]] ConstantBuffer<push_data> push;

struct cs_in {
  uint3 id : SV_DispatchThreadID;
}; // struct cs_in

// Indirect dispatch sized (by build_dispatch_args.slang) to last frame's alive count, rounded up to
// a whole group — the last group can run a few threads past the real count, so bail on those.
[shader("compute")]
[numthreads(64, 1, 1)]
void compute_main(cs_in input) {
  uint index = input.id.x;

  if (index >= push.counters[0].alive_count[push.read_index]) {
    return;
  }

  uint slot = push.alive_list_read[index];
  particle p = push.particles[slot];

  p.age += push.dt;

  if (p.age >= p.lifetime) {
    // Dead: return the slot to the free stack, don't carry it into this frame's alive list.
    uint dead_index;
    InterlockedAdd(push.counters[0].dead_count, 1u, dead_index);

    if (dead_index < push.max_particles) {
      push.dead_list[dead_index] = slot;
    }

    return;
  }

  emitter_instance e = push.emitters[p.emitter_slot];

  p.velocity += float3(0.0, -1.0, 0.0) * e.gravity * push.dt;
  p.velocity *= max(1.0 - e.drag * push.dt, 0.0);
  p.position += p.velocity * push.dt;

  push.particles[slot] = p;

  uint alive_index;
  InterlockedAdd(push.counters[0].alive_count[push.write_index], 1u, alive_index);

  if (alive_index < push.max_particles) {
    push.alive_list_write[alive_index] = slot;
  }
}
